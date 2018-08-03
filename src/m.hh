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

#include <algorithm>
#include <map>
#include <string>
#include <set>
#include <vector>
#include <iostream>
#include <regex>
#include "preamble.hh"

using namespace std::literals::string_literals;

namespace m {
template<typename T, typename ...Args>
class Factory
{
  public:
    static T& create(const std::string& name, const Args&... args)
    {
      auto i = _registry.find(name);
      if(i != _registry.end())
        return *i->second;
      auto t = new T(name, args...);
      _registry.insert(std::make_pair(name, t));
      return *t;
    }
    static const T* get(const std::string& name)
    {
      auto i = _registry.find(name);
      if(i != _registry.end())
        return i->second;
      return nullptr;
    }
  private:
    using map = std::map<std::string, T*>;
    static map _registry;
};

template<typename T, typename... Args>
typename Factory<T, Args...>::map Factory<T, Args...>::_registry;

class expand_type
{
  public:
    template<typename... T>
    expand_type(T&&...) {}
};

template<typename T>
class unique_vector
{
  public:
    unique_vector() {}
    void push_back(const T& t)
    {
      bool b = _set.insert(t).second;
      if(b)
        _vector.push_back(t);
    }
    const std::vector<T>& vector() const { return _vector; }
  private:
    std::set<T> _set;
    std::vector<T> _vector;
};

template<typename T, typename F>
void print(const T& t, std::ostream& out, const std::string& prefix, F f)
{
  if(t.empty())
    return;
  out << prefix;
  std::for_each(std::begin(t), std::end(t), f);
  out << std::endl;
}

class External
{
  public:
    External(const std::string& url, const std::string& hash)
      : _url(url), _hash(hash)
    {
    }
    const std::string& url() const { return _url; }
    const std::string& hash() const { return _hash; }
  private:
    const std::string _url;
    const std::string _hash;
};

class Project;
class Library;

class Object
{
  public:
    Object(const std::string& name) : _name(name), _header_only(true), _compiled(false) {}
    virtual ~Object() {}
    const std::string& name() const { return _name; }
    template<typename ...T>
    void ccflags(T... args)
    {
      expand_type{0, (_ccflags.push_back(args), 0)...};
    }
    template<typename ...T>
    void ldflags(T... args)
    {
      expand_type{0, (_ldflags.push_back(args), 0)...};
    }
    void incs(const std::string& include)
    {
      _include_path.push_back(include);
    }
    virtual void libs(const std::string& lib)
    {
      _library_path.push_back(lib);
      _header_only = false;
    }
    void srcs(const std::string& source)
    {
      _source_path = source;
    }
    void ext(const std::string& extension)
    {
      _extension = extension;
    }
    void add_def(const std::string& def)
    {
      _defines.push_back(def);
    }
    void add_src(const std::string& source)
    {
      _sources.push_back(source);
      _header_only = false;
      _compiled = true;
    }
    void add(const Library& lib)
    {
      _libraries.push_back(&lib);
    }
    const std::vector<std::string>& defines() const { return _defines; }
    const std::vector<std::string>& library_path() const { return _library_path; }
    const std::vector<std::string>& include_path() const { return _include_path; }
    const std::string& src_path() const;
    const std::string& extension(const Project&) const;
    const std::string& rule(const std::string& ext) const
    {
      static const std::string c{"COMPILE.c"};
      static const std::string cc{"COMPILE.cc"};
      if(ext == ".c"s)
        return c;
      else if(ext == ".cc"s)
        return cc;
      else if(ext == ".cpp"s)
        return cc;
      else if(ext == ".cxx"s)
        return cc;
      else if(ext == ".C"s)
        return cc;
      throw std::runtime_error("Unknown extension: "s + ext);
    }
    virtual void generate(std::ostream&, const Project&) const {}
  protected:
    std::vector<std::string> _ccflags;
    std::vector<std::string> _ldflags;
    std::string _source_path;
    std::string _extension;
    std::vector<std::string> _defines;
    std::vector<std::string> _include_path;
    std::vector<std::string> _library_path;
    std::vector<std::string> _sources;
    std::vector<const Library*> _libraries;
    bool _header_only;
    bool _compiled;
  private:
    const std::string _name;
};

class Library : public Object, public Factory<Library>
{
  public:
    Library(const std::string& name) : Object(name) {}
    virtual ~Library() {}
    bool header_only() const { return _header_only; }
    void header_only(bool x) { _header_only = x; }
    bool compiled() const { return _compiled; }
    void url(const std::string& url, const std::string& hash)
    {
      _external.reset(new External(url, hash));
    }
    void fetch(const Project& project) const;
    virtual void generate(std::ostream& out, const Project& project) const override
    {
      fetch(project);
      if(!_sources.empty())
      {
        out << std::endl << "# lib: " << name() << std::endl;
        unique_vector<std::string> defines_v;
        unique_vector<std::string> includes_v;
        for(const auto& def: defines())
          defines_v.push_back(def);
        for(const auto& inc: include_path())
          includes_v.push_back(inc);
        for(const auto& l: _libraries)
        {
          for(const auto& inc: l->include_path())
            includes_v.push_back(inc);
        }
        for(const auto& i: _sources)
        {
          std::string src = src_path();
          if(!src.empty())
            src += '/';
          out << "build $builddir/obj/" << name() << "/" << i << ".o: "
            << rule(extension(project)) << " $topdir/"
            << src << i << extension(project) << std::endl;
          print(_ccflags, out, " ccflags =", [&out](const auto& s) { out << " " << s; });
          print(defines_v.vector(), out, " -D =", [&out](const auto& s) { out << " " << s; });
          print(includes_v.vector(), out, " -I =",
            [&out](const auto& s) {
              if(s[0] == '/')
                out << " -I" << s;
              else
                out << " -I$topdir/" << s;
            });
        }
        out << "build lib" << name() << ".a: phony $builddir/lib/lib" << name() << ".a" << std::endl;
        out << "build $builddir/lib/lib" << name() << ".a: ARCHIVE";
        for(const auto& i: _sources)
          out << " $builddir/obj/" << name() << "/" << i << ".o";
        out << std::endl;
      }
    }
  private:
    std::shared_ptr<External> _external;
};

class Template : public Library, public Factory<Template>
{
  public:
    Template(const std::string& name, const std::string& template_)
      : Library(name), _template(template_)
    {}
    virtual ~Template() {}
    virtual void libs(const std::string& lib)
    {
      _library_path.push_back(lib);
    }
    const Library& library(const std::string& library_name) const
    {
      auto libraryp = Factory<Library>::get(template_name(library_name));
      if(libraryp)
        return *libraryp;
      auto& library = Factory<Library>::create(template_name(library_name));
      for(auto i: _defines)
        library.add_def(i);
      for(auto i: _include_path)
        library.incs(i);
      for(auto i: _library_path)
        library.libs(i);
      library.header_only(false);
      return library;
    }
  private:
    const std::string template_name(const std::string& name) const
    {
      static const std::regex re("%");
      return std::regex_replace(_template, re, name);
    }
    const std::string _template;
};

class Binary : public Object, public Factory<Binary>
{
  public:
    Binary(const std::string& name) : Object(name) {}
    virtual ~Binary() {}
    virtual void generate(std::ostream& out, const Project& project) const override
    {
      out << std::endl << "# bin: " << name() << std::endl;
      unique_vector<std::string> libs_v;
      unique_vector<std::string> defines_v;
      unique_vector<std::string> includes_v;
      unique_vector<std::string> libsearch_v;
      unique_vector<std::string> deps_v;
      for(const auto& def: defines())
        defines_v.push_back(def);
      for(const auto& inc: include_path())
        includes_v.push_back(inc);
      for(const auto& l: _libraries)
      {
        {
          if(!l->header_only())
          {
            libs_v.push_back(l->name());
            if(l->compiled())
              deps_v.push_back(l->name());
          }
        }
        for(const auto& inc: l->include_path())
          includes_v.push_back(inc);
        for(const auto& lib: l->library_path())
          libsearch_v.push_back(lib);
      }
      for(const auto& i: _sources)
      {
        std::string src = src_path();
        if(!src.empty())
          src += '/';
        out << "build $builddir/obj/" << name() << "/" << i << ".o: "
          << rule(extension(project)) << " $topdir/"
          << src << i << extension(project) << std::endl;
        print(_ccflags, out, " ccflags =", [&out](const auto& s) { out << " " << s; });
        print(defines_v.vector(), out, " -D =", [&out](const auto& s) { out << " " << s; });
        print(includes_v.vector(), out, " -I =",
          [&out](const auto& s) {
            if(s[0] == '/')
              out << " -I" << s;
            else
              out << " -I$topdir/" << s;
          });
      }
      out << "build " << name() << ": phony $builddir/bin/" << name() << std::endl;
      out << "build $builddir/bin/" << name() << ": LINK.cc";
      for(const auto& i: _sources)
        out << " $builddir/obj/" << name() << "/" << i << ".o";
      print(deps_v.vector(), out, " |", [&out](const auto& s) { out << " $builddir/lib/lib" << s << ".a"; });
      if(deps_v.vector().empty())
        out << std::endl;
      print(_ldflags, out, " ldflags =", [&out](const auto& s) { out << " " << s; });
      print(libsearch_v.vector(), out, " -L =", [&out](const auto& s) { out << " -L" << s; });
      print(libs_v.vector(), out, " -l =", [&out](const auto& s) { out << " -l" << s; });
    }
};

class Project
{
  public:
    Project(const std::string& name, const std::string& topdir, const std::string& builddir)
      : _name(name), _topdir(topdir), _builddir(builddir) {}
    // No copy allowed.
    Project(const Project& o) = delete;
    Project(Project&& o) noexcept
      : _name(o._name), _topdir(o._topdir), _builddir(o._builddir),
        _ccflags(o._ccflags), _ldflags(o._ldflags),
        _source_path(o._source_path), _extension(o._extension),
        _include_path(o._include_path), _library_path(o._library_path),
        _binaries(o._binaries), _libraries(o._libraries)
    {
    }
    ~Project()
    {
    }
    const std::string& name() const { return _name; }
    const std::string& topdir() const { return _topdir; }
    const std::string& builddir() const { return _builddir; }
    void fetch(const std::string& name, const std::string& url, const std::string& hash) const;
    template<typename ...T>
    void ccflags(T... args)
    {
      expand_type{0, (_ccflags.push_back(args), 0)...};
    }
    template<typename ...T>
    void ldflags(T... args)
    {
      expand_type{0, (_ldflags.push_back(args), 0)...};
    }
    void incs(const std::string& include)
    {
      _include_path.push_back(include);
    }
    void libs(const std::string& library)
    {
      _library_path.push_back(library);
    }
    void srcs(const std::string& source)
    {
      _source_path = source;
    }
    void ext(const std::string& extension)
    {
      _extension = extension;
    }
    void add(const Binary& bin)
    {
      _binaries.push_back(&bin);
    }
    void add(const Library& lib)
    {
      _libraries.push_back(&lib);
    }
    const std::string& extension() const
    {
      if(!_extension.empty())
        return _extension;
      static const std::string default_extension{".cc"};
      return default_extension;
    }
    const std::string& src_path() const
    {
      return _source_path;
    }
    void generate(std::ostream& out) const
    {
      out << preamble[0] << std::endl << std::endl;
      out << "topdir = " << _topdir << std::endl;
      out << "builddir = " << _builddir << std::endl;
      print(_ccflags, out, "ccflags =", [&out](const auto& s) { out << " " << s; });
      print(_ldflags, out, "ldflags =", [&out](const auto& s) { out << " " << s; });
      print(_include_path, out, "incs =", [&out](const auto& s) {
          out << " -I";
          if(s[0] == '/')
            out << s;
          else
            out << "$topdir/" << s;
        });
      out << std::endl << preamble[1] << std::endl;
      for(const auto& i: _libraries)
      {
        i->generate(out, *this);
      }
      for(const auto& i: _binaries)
      {
        i->generate(out, *this);
      }
    }
  private:
    const std::string _name;
    const std::string _topdir;
    const std::string _builddir;
    std::vector<std::string> _ccflags;
    std::vector<std::string> _ldflags;
    std::string _source_path;
    std::string _extension;
    std::vector<std::string> _include_path;
    std::vector<std::string> _library_path;
    std::vector<const Binary*> _binaries;
    std::vector<const Library*> _libraries;
};

class BuilderBase
{
  public:
    BuilderBase(Project& project)
      : _project(project)
    {}
    virtual ~BuilderBase() {}
    operator Project() { delete _current; _current = nullptr; return std::move(_project); }
    Project& project() { return _project; }
    BuilderBase& lib(const std::string& name);
    BuilderBase& lib(const std::string& name, const std::string& pattern);
    BuilderBase& bin(const std::string& name);
    template<typename ...T>
    BuilderBase& ccflags(T... arg)
    {
      expand_type{0, (ccflag(arg), 0)...};
      return *this;
    }
    template<typename ...T>
    BuilderBase& ldflags(T... arg)
    {
      expand_type{0, (ldflag(arg), 0)...};
      return *this;
    }
    virtual void ccflag(const std::string&) { error("ccflags"); }
    virtual void ldflag(const std::string&) { error("ldflags"); }
    virtual BuilderBase& incs(const std::string&) { return error("incs"); }
    virtual BuilderBase& libs(const std::string&) { return error("libs"); }
    virtual BuilderBase& srcs(const std::string&) { return error("srcs"); }
    virtual BuilderBase& ext(const std::string&) { return error("ext"); }
    virtual BuilderBase& url(const std::string&, const std::string&) { return error("url"); }
    virtual BuilderBase& add_src(const std::string&) { return error("add_src"); }
    virtual BuilderBase& add_def(const std::string&) { return error("add_def"); }
    virtual BuilderBase& add_lib(const std::string&) { return error("add_lib"); }
    virtual BuilderBase& add_lib(const std::string&, const std::string&) { return error("add_lib"); }
  protected:
    virtual void close() {}
    Project& _project;
    static BuilderBase* _current;
  private:
    BuilderBase& error(const std::string& message)
    {
      throw std::runtime_error(message + ": not available for this object");
    }
};

class ProjectBuilder : public BuilderBase
{
  public:
    ProjectBuilder(const std::string& name, const std::string& top, const std::string& build)
      : BuilderBase(project), project(name, top, build)
    {}
    virtual ~ProjectBuilder() { delete _current; }
    static ProjectBuilder& create(const std::string& name, const std::string& top,
      const std::string& build);

    virtual void ccflag(const std::string& flag) { project.ccflags(flag); }
    virtual void ldflag(const std::string& flag) { project.ldflags(flag); }
    virtual BuilderBase& incs(const std::string& inc)
    {
      project.incs(inc);
      return *this;
    }
    virtual BuilderBase& libs(const std::string& lib)
    {
      project.libs(lib);
      return *this;
    }
    virtual BuilderBase& srcs(const std::string& src)
    {
      project.srcs(src);
      return *this;
    }
    virtual BuilderBase& ext(const std::string& e)
    {
      project.ext(e);
      return *this;
    }

  private:
    Project project;
};

class LibraryBuilder : public BuilderBase
{
  public:
    LibraryBuilder(Project& project, const std::string& name)
      : BuilderBase(project), _library(Factory<Library>::create(name))
    {
      _library.srcs(project.src_path());
    }
    virtual ~LibraryBuilder() { close(); }
    virtual void ccflag(const std::string& flag) { _library.ccflags(flag); }
    virtual void ldflag(const std::string& flag) { _library.ldflags(flag); }
    virtual BuilderBase& incs(const std::string& inc)
    {
      _library.incs(inc);
      return *this;
    }
    virtual BuilderBase& libs(const std::string& lib)
    {
      _library.libs(lib);
      return *this;
    }
    virtual BuilderBase& srcs(const std::string& src)
    {
      _library.srcs(src);
      return *this;
    }
    virtual BuilderBase& ext(const std::string& extension)
    {
      _library.ext(extension);
      return *this;
    }
    virtual BuilderBase& url(const std::string& adr, const std::string& hash)
    {
      _library.url(adr, hash);
      return *this;
    }
    virtual BuilderBase& add_src(const std::string& src)
    {
      _library.add_src(src);
      _library.libs("$builddir/lib");
      return *this;
    }
    virtual BuilderBase& add_def(const std::string& def)
    {
      _library.add_def(def);
      return *this;
    }
    virtual BuilderBase& add_lib(const std::string& lib)
    {
      auto* l0 = Factory<Template, std::string>::get(lib);
      if(l0)
        _library.add(*l0);
      else
      {
        auto* l1 = Factory<Library>::get(lib);
        if(l1)
          _library.add(*l1);
      }
      return *this;
    }
    virtual BuilderBase& add_lib(const std::string& parent, const std::string& lib)
    {
      auto* t = Factory<Template, std::string>::get(parent);
      if(t)
      {
        auto& l = t->library(lib);
        _library.add(l);
      }
      return *this;
    }
  private:
    virtual void close()
    {
      _project.add(_library);
    }
    Library& _library;
};

class BinaryBuilder : public BuilderBase
{
  public:
    BinaryBuilder(Project& project, const std::string& name)
      : BuilderBase(project), _binary(Factory<Binary>::create(name))
    {
      _binary.srcs(project.src_path());
    }
    virtual void ccflag(const std::string& flag) { _binary.ccflags(flag); }
    virtual void ldflag(const std::string& flag) { _binary.ldflags(flag); }
    virtual BuilderBase& incs(const std::string& inc)
    {
      _binary.incs(inc);
      return *this;
    }
    virtual BuilderBase& libs(const std::string& lib)
    {
      _binary.libs(lib);
      return *this;
    }
    virtual BuilderBase& srcs(const std::string& src)
    {
      _binary.srcs(src);
      return *this;
    }
    virtual BuilderBase& ext(const std::string& extension)
    {
      _binary.ext(extension);
      return *this;
    }
    virtual BuilderBase& add_src(const std::string& src)
    {
      _binary.add_src(src);
      return *this;
    }
    virtual BuilderBase& add_def(const std::string& def)
    {
      _binary.add_def(def);
      return *this;
    }
    virtual BuilderBase& add_lib(const std::string& lib)
    {
      auto* l0 = Factory<Template, std::string>::get(lib);
      if(l0)
        _binary.add(*l0);
      else
      {
        auto* l1 = Factory<Library>::get(lib);
        if(l1)
          _binary.add(*l1);
      }
      return *this;
    }
    virtual BuilderBase& add_lib(const std::string& parent, const std::string& lib)
    {
      auto* t = Factory<Template, std::string>::get(parent);
      if(t)
      {
        auto& l = t->library(lib);
        _binary.add(l);
      }
      return *this;
    }
    virtual ~BinaryBuilder() { close(); }
  private:
    virtual void close()
    {
      _project.add(_binary);
    }
    Binary& _binary;
};

class TemplateBuilder : public BuilderBase
{
  public:
    TemplateBuilder(Project& project, const std::string& name, const std::string& pattern)
      : BuilderBase(project), _template(Factory<Template, std::string>::create(name, pattern))
    {}
    virtual ~TemplateBuilder() { close(); }
    virtual BuilderBase& incs(const std::string& inc)
    {
      _template.incs(inc);
      return *this;
    }
    virtual BuilderBase& libs(const std::string& lib)
    {
      _template.libs(lib);
      return *this;
    }
    virtual BuilderBase& srcs(const std::string& src)
    {
      _template.srcs(src);
      return *this;
    }
  private:
    virtual void close()
    {
      _project.add(_template);
    }
    Template& _template;
};
}
