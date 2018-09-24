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

BuilderBase& BuilderBase::frameworks(const std::string& name, const std::string& path)
{
  delete _current;
  _current = new FrameworkBuilder(_project, name, path);
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

class Git
{
  public:
    Git() : _git(bp::search_path("git")) {}
    int clone(const std::string& url, const fs::path& location)
    {
      return bp::system(_git, "clone", url, location);
    }
    int summary(const std::string& ref)
    {
      return bp::system(_git, "show", "--summary", ref, bp::std_out > bp::null);
    }
    int fetch(const std::string& from)
    {
      return bp::system(_git, "fetch", from);
    }
    const std::string get_hash(const std::string& ref)
    {
      std::future<std::string> os;
      bp::system(_git, "log", "--pretty=format:%H", "-1", ref, bp::std_out > os);
      return os.get();
    }
    int reset_hard(const std::string& ref)
    {
      return bp::system(_git, "reset", "--hard", ref);
    }
  private:
    fs::path _git;
};

void Project::fetch(const std::string& name, const std::string& url, const std::string& hash_or_tag) const
{
  fs::path location = _topdir;
  location /= ".externals";
  location /= name;
  Git git;
  if(!fs::exists(location))
  {
    std::cout << "Cloning " << url << std::endl;
    git.clone(url, location);
  }
  auto current_path = fs::current_path();
  fs::current_path(location);
  auto ec = git.summary(hash_or_tag);
  if(ec != 0)
  {
    std::cout << "Fetching " << location << std::endl;
    git.fetch("origin");
  }
  auto head = git.get_hash("HEAD");
  auto hash = git.get_hash(hash_or_tag);
  if(head != hash)
  {
    std::cout << "Reset " << location << std::endl;
    git.reset_hard(hash_or_tag);
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
    m::Loader loader(topdir, builddir);
    m::Project p = loader.load_file(_m);
    std::ofstream out{"build.ninja"};
    if(!out)
      throw std::runtime_error("Can't open build.ninja for writing");
    p.generate(out);
    fs::path ninja = bp::search_path("ninja");
    if(ninja.empty())
      throw std::runtime_error("Can't find program 'ninja'");
    bp::system(ninja, args);
  }
  catch(const std::runtime_error& e)
  {
    std::cerr << e.what() << std::endl;
  }
  return 0;
}
