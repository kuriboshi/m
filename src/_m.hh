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

#pragma once

#include <iostream>
#include <boost/filesystem.hpp>
#include "m.hh"

namespace fs = boost::filesystem;

namespace m {
class Loader
{
  public:
    Loader(const std::string& topdir = ".", const std::string& builddir = "build")
      : _topdir(topdir), _builddir(builddir)
    {}
    BuilderBase& load_file(const std::string& file, BuilderBase* initial_builder = nullptr);
  private:
    void find_files(const fs::path& dir, const std::string& file, std::set<std::string>& result);
    const std::string _topdir;
    const std::string _builddir;
};
}
