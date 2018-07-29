// Copyright 2018 Krister Joas <krister@joas.jp>

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string>
#include <iostream>
#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include "m.hh"
#include "_m.hh"

using namespace std::literals::string_literals;
namespace fs = boost::filesystem;
namespace bp = boost::process;

namespace m {
const std::string& Object::extension(const Project& project) const
{
  if(!_extension.empty())
    return _extension;
  return project.extension();
}

const std::string& Object::src_path() const
{
  return _source_path;
}

void Library::fetch(const Project& project) const
{
  if(_external)
    project.fetch(name(), _external->url(), _external->hash());
}

BuilderBase& BuilderBase::lib(const std::string& name)
{
  delete _current;
  _current = new LibraryBuilder(_project, name);
  return *_current;
}

BuilderBase& BuilderBase::lib(const std::string& name, const std::string& pattern)
{
  delete _current;
  _current = new TemplateBuilder(_project, name, pattern);
  return *_current;
}

BuilderBase& BuilderBase::bin(const std::string& name)
{
  delete _current;
  _current = new BinaryBuilder(_project, name);
  return *_current;
}

BuilderBase* BuilderBase::_current = nullptr;

ProjectBuilder& ProjectBuilder::create(const std::string& name, const std::string& top,
  const std::string& build)
{
  return *new ProjectBuilder(name, top, build);
}

void Project::fetch(const std::string& name, const std::string& url, const std::string& hash) const
{
  fs::path location = _topdir;
  location /= ".externals";
  location /= name;
  auto git = bp::search_path("git");
  if(!fs::exists(location))
  {
    std::cout << "Cloning " << url << std::endl;
    bp::system(git, "clone", url, location);
  }
  auto current_path = fs::current_path();
  fs::current_path(location);
  auto ec = bp::system(git, "show", "--summary", hash, bp::std_out > bp::null);
  if(ec != 0)
  {
    std::cout << "Fetching " << location << std::endl;
    bp::system(git, "fetch", "origin");
  }
  std::future<std::string> os;
  ec = bp::system(git, "show", "--pretty=format:%H", "--no-patch", bp::std_out > os);
  if(os.get() != hash)
  {
    std::cout << "Reset " << location << std::endl;
    bp::system(git, "reset", "--hard", hash);
  }
  fs::current_path(current_path);
}
}

int main(int argc, const char** argv)
{
  std::string topdir{"."};
  std::string builddir{"build"};
  std::string _m{"_m"};
  int start = 1;
  if(argc > 1)
  {
    fs::path top{argv[1]};
    top /= "_m";
    if(fs::is_regular_file(top))
    {
      topdir = argv[1];
      builddir = ".";
      _m = top.string();
      ++start;
    }
  }
  const std::vector<std::string> args{argv + start, argv + argc};
  try
  {
    {
      m::Loader loader(topdir, builddir);
      m::Project p = *loader.load_file(_m);
      std::ofstream out{"build.ninja"};
      p.generate(out);
    }
    fs::path ninja = bp::search_path("ninja");
    int result = bp::system(ninja, args);
  }
  catch(const std::runtime_error& e)
  {
    std::cerr << e.what() << std::endl;
  }
  return 0;
}
