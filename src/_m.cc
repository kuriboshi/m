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

#include <algorithm>
#include <iostream>
#include <fstream>
#include <regex>
#include <string>
#include <vector>
#include <boost/filesystem.hpp>

#include "m.hh"
#include "_m.hh"

using namespace std::literals::string_literals;

namespace m {
BuilderBase* Loader::load_file(const std::string& file, BuilderBase* initial_builder)
{
  const std::regex ws{"\\s+"};
  const std::regex comment{"#.*"};
  std::ifstream in{file};
  BuilderBase* builder = initial_builder;
  if(initial_builder != nullptr)
    initial_builder->project().srcs(fs::path(file).parent_path().string());
  std::string line;
  int line_count = 0;
  for(std::string s; std::getline(in, s);)
  {
    ++line_count;
    std::smatch sm;
    if(std::regex_search(s, sm, ws) && sm.prefix().str().empty())
      s = sm.suffix().str();
    if(std::regex_search(s, sm, comment))
      s = sm.prefix().str();
    if(s.empty())
      continue;
    if(s.back() == '\\')
    {
      s.pop_back();
      line += s;
      continue;
    }
    line += s;
    std::vector<std::string> result{
      std::sregex_token_iterator(std::begin(line), std::end(line), ws, -1), {}
    };
    auto size = result.size();
    const auto& directive = result[0];
    if(directive == "project"s && size == 2)
      builder = &ProjectBuilder::create(result[1], _topdir, _builddir);
    else if(directive == "ccflags"s && size >= 1)
    {
      for(int i = 1; i != result.size(); ++i)
        builder = &builder->ccflags(result[i]);
    }
    else if(directive == "ldflags"s && size >= 1)
    {
      for(int i = 1; i != result.size(); ++i)
        builder = &builder->ldflags(result[i]);
    }
    else if(directive == "incs"s && size == 2)
      builder = &builder->incs(result[1]);
    else if(directive == "libs"s && size == 2)
      builder = &builder->libs(result[1]);
    else if(directive == "srcs"s && size == 2)
      builder = &builder->srcs(result[1]);
    else if(directive == "ext"s && size == 2)
      builder = &builder->ext(result[1]);
    else if(directive == "url"s && size == 3)
      builder = &builder->url(result[1], result[2]);
    else if(directive == "lib"s && size == 2)
      builder = &builder->lib(result[1]);
    else if(directive == "lib"s && size == 3)
      builder = &builder->lib(result[1], result[2]);
    else if(directive == "bin"s && size == 2)
      builder = &builder->bin(result[1]);
    else if(directive == "add"s)
    {
      const auto& sub = result[1];
      if(sub == "src"s && size == 3)
        builder = &builder->add_src(result[2]);
      else if(sub == "lib"s && size == 3)
        builder = &builder->add_lib(result[2]);
      else if(sub == "lib"s && size == 4)
        builder = &builder->add_lib(result[2], result[3]);
      else if(sub == "def"s && size == 3)
        builder = &builder->add_def(result[2]);
    }
    else if(directive == "load"s && size == 2)
      builder = load_file(result[1], builder);
    else if(directive == "subdirs"s && size == 2)
    {
      std::set<std::string> list;
      find_files(result[1], "_m", list);
      for(auto _m: list)
        builder = load_file(_m, builder);
    }
    else
    {
      std::cerr << "illegal directive line " << line_count << ":";
      for(auto& i: result)
        std::cerr << " " << i;
      std::cerr << std::endl;
    }
    line.clear();
  }
  return builder;
}

void Loader::find_files(const fs::path& dir, const std::string& file, std::set<std::string>& result)
{
  if(!fs::exists(dir))
    return;
  for(auto d: fs::recursive_directory_iterator(dir))
    if(fs::is_regular_file(d.status()) && d.path().filename() == file)
      result.insert(d.path().string());
}
}
