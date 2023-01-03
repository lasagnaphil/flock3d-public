from typing import Optional
from copy import copy, deepcopy

def _convert_arg_to_list(arg) -> list:
    if arg is None:
        return []
    elif isinstance(arg, str):
        return [arg]
    elif isinstance(arg, list):
        return arg
    else:
        raise RuntimeError("_convert_arg_to_list failed")

def _conv_fbuild(value) -> str:
    if isinstance(value, str):
        return _conv_fbuild_str(value)
    elif isinstance(value, bool):
        return _conv_fbuild_bool(value)
    elif isinstance(value, list):
        if len(value) == 0:
            return ""
        elif len(value) == 1:
            return _conv_fbuild(value[0])
        else:
            return '{' + ', '.join([_conv_fbuild(v) for v in value]) + '}'
    else:
        raise RuntimeError("_conv_fbuild failed")

def _conv_fbuild_str(value) -> str:
    return f"'{value}'"

def _conv_fbuild_bool(b: bool) -> str:
    if b: return 'true'
    else: return 'false'

class FBuildFileWriter:
    def __init__(self, f):
        self.f = f
        self.indent_depth = 0
        self.brace_opened = False

    def begin_brace(self):
        self.f.write('\t' * self.indent_depth + '{\n')
        self.indent_depth += 1

    def end_brace(self):
        self.indent_depth -= 1
        self.f.write('\t' * self.indent_depth + '}\n')

    def write(self, s: str):
        self.f.write('\t' * self.indent_depth + s)

    def writeln(self, s: str):
        self.f.write('\t' * self.indent_depth + s + '\n')

    def write_without_spacing(self, s: str):
        self.f.write(s)

    def writeln_without_spacing(self, s: str):
        self.f.write(s + '\n')

    def newline(self):
        self.f.write('\t' * self.indent_depth + '\n')

    def var(self, name: str, value):
        self.writeln(f".{name} = {_conv_fbuild(value)}")

    def var_append(self, name: str, value):
        self.writeln(f".{name} + {_conv_fbuild(value)}")

    def using(self, value):
        self.writeln(f'Using({value})')

    def function(self, command: str, param: str):
        return FBuildFunctionContext(self, command, param)

    def for_each(self, item: str, itemlist: str):
        return FBuildFunctionContext(self, "ForEach", f"{item} in {itemlist}")

    def do_if(self, conditional: str):
        return FBuildFunctionContext(self, "If", conditional)

    def target(self, command: str, name: str):
        return FBuildFunctionContext(self, command, f"'{name}'")

class FBuildFunctionContext:
    def __init__(self, writer: FBuildFileWriter, command: str, param: str):
        self.writer = writer
        self.command = command
        self.param = param

    def __enter__(self):
        self.writer.writeln(f"{self.command}({self.param})")
        self.writer.begin_brace()
        return self

    def __exit__(self, *exc_info):
        self.writer.end_brace()

class FBuildPreprocessorConditionalContext:
    def __init__(self, writer: FBuildFileWriter, conditional: str):
        self.writer = writer
        self.conditional = conditional

    def __enter__(self):
        self.writer.writeln_without_spacing(f"#if {self.conditional}")
        return self

    def __exit__(self, *exc_info):
        self.writer.writeln_without_spacing("#endif")

class Compiler:
    def __init__(self,
                 name: str,
                 executable: str,
                 family: str,
                 allow_distribution: bool=True):
        self.name = name
        self.executable = executable
        self.family = family
        self.allow_distribution = allow_distribution

    def _write_globals(self, writer: FBuildFileWriter):
        pass

    def _write_function(self, writer: FBuildFileWriter):
        with writer.function('Compiler', f"'{self.name}'"):
            writer.var("Executable", self.executable)
            writer.var("CompilerFamily", self.family)
            writer.var("AllowDistribution", self.allow_distribution)

class Target:
    def __init__(
        self,
        name: str,
        deps: Optional[list["Target"]],
        build_config_dependent: bool):

        self.name = name
        self.deps = _convert_arg_to_list(deps)
        self.build_config_dependent = build_config_dependent

        self._all_deps: set[Target] = set()
        self._marked = False

    def __repr__(self) -> str:
        return f"{type(self).__name__}({self.name})"

    @property
    def fbuild_name(self) -> str:
        name = self.name
        if self.build_config_dependent:
            name += '-$Platform$-$BuildConfigName$'
        return name

    def _write_globals(self, writer: FBuildFileWriter):
        pass

    def _write_function(self, writer: FBuildFileWriter):
        pass

class CompileTarget(Target):
    def __init__(
            self,
            name: str,
            basepath: str,
            deps=None,
            defines=None,
            includes=None,
            pub_defines=None,
            pub_includes=None,
            priv_defines=None,
            priv_includes=None,
            build_config_dependent: bool=True):

        super(CompileTarget, self).__init__(name, deps, build_config_dependent)

        self.basepath = basepath

        self.pub_defines = _convert_arg_to_list(defines or pub_defines)
        self.pub_includes = _convert_arg_to_list(includes or pub_includes)
        self.priv_defines = _convert_arg_to_list(priv_defines)
        self.priv_includes = _convert_arg_to_list(priv_includes)

    def _write_globals(self, writer: FBuildFileWriter):
        writer.var(f"{self.name}_BasePath", self.basepath)

        define_flags = ''.join(f' "-D{define}"' for define in (self.pub_defines + self.priv_defines))
        define_flags += ''.join(f' ${dep.fbuild_define_flag}$' for dep in self._all_deps if isinstance(dep, CompileTarget))
        writer.var(self.fbuild_define_flag, define_flags)

        include_flags = ''.join(f' "-I${self.fbuild_basepath}$/{dir}"' for dir in (self.pub_includes + self.priv_includes))
        include_flags += ''.join(f' ${dep.fbuild_include_path_flag}$' for dep in self._all_deps if isinstance(dep, CompileTarget))
        writer.var(self.fbuild_include_path_flag, include_flags)

    def _write_function(self, writer: FBuildFileWriter):
        pass

    @property
    def fbuild_basepath(self) -> str:
        return f"{self.name}_BasePath"

    @property
    def fbuild_define_flag(self) -> str:
        return f"{self.name}_DefineFlag"

    @property
    def fbuild_include_path_flag(self) -> str:
        return f"{self.name}_IncludePathFlag"

class Project:
    def __init__(self,
                 name: str,
                 platform: str):
        self.name = name
        self.platform = platform
        self.targets: dict[str, Target] = dict()
        self.compilers: dict[str, Compiler] = dict()

    def add_target(self, target: Target):
        if isinstance(target, Target):
            if target.name in self.targets:
                raise RuntimeError(f"Error while adding target to project: same name {target.name} already exists in project!")
            self.targets[target.name] = target
        else:
            raise TypeError("add_target() received a non-Target object as parameter!")

    def add_targets(self, targets: list[Target]):
        for target in targets:
            self.add_target(target)

    def add_compiler(self, compiler: Compiler):
        if isinstance(compiler, Compiler):
            if compiler.name in self.compilers:
                raise RuntimeError(f"Error while adding compiler to project: same name {compiler.name} already exists in project!")
            self.compilers[compiler.name] = compiler
        else:
            raise TypeError("add_compiler() received a non-Compiler object as parameter!")

    def default_obj_ext(self):
        if self.platform == 'windows':
            return '.obj'
        elif self.platform == 'macos' or self.platform == 'linux':
            return '.a'

    @property
    def output_path(self):
        return "_out/$BuildConfigName$"

    @property
    def binary_path(self):
        return "_bin/$BuildConfigName$"

    def generate(self):
        if len(self.targets) == 0:
            raise RuntimeError(f"Error while generating: there are no targets in the project!")

        # Resolve all dependencies
        def toposort_targets(target_list):
            # Topological sort via DFS.
            output = []

            def visit(target):
                if target._marked: return

                for dep_target in target.deps:
                    visit(dep_target)

                target._marked = True
                output.append(target)

            for target in target_list:
                visit(target)

            return output

        sorted_targets = toposort_targets(self.targets.values())

        for target in sorted_targets:
            target._all_deps = set(target.deps)
            for dep_target in target.deps:
                target._all_deps.update(dep_target._all_deps)

            print(f"""Target {target.name}: 
    Deps = {sorted(target.deps, key=lambda t: t.name)}
    All Deps = {sorted(target._all_deps, key=lambda t: t.name)}""")

        build_config_agnostic_targets = list()
        build_config_specific_targets = list()
        for target in sorted_targets:
            if target.build_config_dependent:
                build_config_specific_targets.append(target)
            else:
                build_config_agnostic_targets.append(target)

        f = open('fbuild.bff', 'w')
        writer = FBuildFileWriter(f)

        writer.writeln('#include "FastBuildSDK/Prelude.bff"')
        writer.newline()

        for compiler in self.compilers.values():
            compiler._write_function(writer)

        for target in sorted_targets:
            target._write_globals(writer)
            writer.newline()

        for target in build_config_agnostic_targets:
            target._write_function(writer)

        with writer.for_each('.BuildConfig', '.BuildConfigs'):
            writer.using('.BuildConfig')
            for target in build_config_specific_targets:
                target._write_function(writer)

        # writer.writeln('#include "FastBuildSDK/Epilogue.bff"')
        f.close()

class ExternalLibrary(CompileTarget):
    def __init__(self,
                 name: str,
                 basepath: str,
                 deps=None,
                 defines=None,
                 includes=None,
                 pub_defines=None,
                 pub_includes=None,
                 libs=None):

        super(ExternalLibrary, self).__init__(name, basepath, deps, defines, includes, pub_defines, pub_includes, None, None, False)

        self.libs = _convert_arg_to_list(libs)

    def _write_globals(self, writer: FBuildFileWriter):
        super(ExternalLibrary, self)._write_globals(writer)

        # link_flags = ' '.join([f'"-L{dir}"' for dir in self._all_lib_dirs])
        # writer.var(f"{self.name}_LinkPathFlag", link_flags)

class HeaderOnlyLibrary(CompileTarget):
    def __init__(self,
                 name: str,
                 basepath: str,
                 deps=None,
                 defines=None,
                 includes=None,
                 pub_defines=None,
                 pub_includes=None):

        super(HeaderOnlyLibrary, self).__init__(name, basepath, deps, defines, includes, pub_defines, pub_includes, None, None, False)

class ObjectList(CompileTarget):
    def __init__(self,
                 name: str,
                 basepath: str,
                 deps=None,
                 compile_as_c:bool=False,
                 source_patterns=None,
                 source_files=None,
                 source_path=None,
                 source_path_recurse:bool=True,
                 dest_path=None,
                 dest_extension=None,
                 dest_keep_base_extension=None,
                 defines=None,
                 includes=None,
                 pub_defines=None,
                 pub_includes=None,
                 priv_defines=None,
                 priv_includes=None,
                 build_config_dependent: bool=True,
                 compiler: Optional[Compiler]=None,
                 compiler_options: Optional[str]=None,
                 use_archive: bool=False):

        super(ObjectList, self).__init__(name, basepath, deps, 
            defines, includes, pub_defines, pub_includes, priv_defines, priv_includes,
            build_config_dependent)

        self.compile_as_c = compile_as_c

        self.source_path = _convert_arg_to_list(source_path)
        self.source_patterns = _convert_arg_to_list(source_patterns)
        if dest_path:
            self.dest_path = dest_path
        else:
            self.dest_path = f'_out/$BuildConfigName$/{self.name}'
        self.source_files = _convert_arg_to_list(source_files)
        self.dest_extension = dest_extension
        self.dest_keep_base_extension = dest_keep_base_extension
        self.source_path_recurse = source_path_recurse
        self.compiler = compiler
        self.compiler_options = compiler_options
        self.use_archive = use_archive

    def _write_function(self, writer: FBuildFileWriter):
        with writer.target('Library' if self.use_archive else 'ObjectList', self.fbuild_name):
            if self.compiler:
                writer.var('Compiler', self.compiler.name)
            if self.source_path:
                writer.var('CompilerInputPath', self.source_path)
            if self.source_patterns:
                writer.var('CompilerInputPattern', self.source_patterns)
            if self.source_files:
                writer.var('CompilerInputFiles', [f"${self.name}_BasePath$/{file}" for file in self.source_files])

            compiler_options_str = f" ${self.name}_DefineFlag$ ${self.name}_IncludePathFlag$"
            if self.compiler_options:
                compiler_options_str += f" {self.compiler_options}"

            # If using custom compiler, need to specify CompilerOptions differently
            if self.compiler:
                writer.var('CompilerOptions', compiler_options_str)
            else:
                if self.compile_as_c:
                    writer.var('CompilerOptions', "$CompilerOptionsC$ " + compiler_options_str)
                else:
                    writer.var_append('CompilerOptions', compiler_options_str)

            if self.dest_extension:
                writer.var('CompilerOutputExtension', self.dest_extension)
            if self.dest_keep_base_extension:
                writer.var('CompilerOutputKeepBaseExtension', self.dest_keep_base_extension)
            writer.var('CompilerOutputPath', self.dest_path)

    @property
    def fbuild_name(self) -> str:
        # Don't use $Platform$-$BuildConfigName$ suffix when using custom compiler
        if self.compiler:
            return self.name
        else:
            return f"{self.name}-$Platform$-$BuildConfigName$"


class Executable(Target):
    def __init__(self,
                 name: str,
                 dest: str,
                 subsystem: str = 'console',
                 additional_libs: Optional[list[str]]=None,
                 deps=None):

        super(Executable, self).__init__(name, deps, True)
        self.dest = dest
        self.subsystem = subsystem
        self.additional_libs = _convert_arg_to_list(additional_libs)

    def _write_globals(self, writer: FBuildFileWriter):
        pass

    def _write_function(self, writer: FBuildFileWriter):
        with writer.target('Executable', self.fbuild_name):
            writer.var('Libraries', 
                [dep.fbuild_name
                for dep in self._all_deps if isinstance(dep, ObjectList)] + 
                [f"{dep.basepath}/{lib}"
                for dep in self._all_deps if isinstance(dep, ExternalLibrary) for lib in dep.libs])
            writer.var('LinkerOutput', self.dest)
            if self.subsystem == 'console':
                linker_options = ' /subsystem:console $CRTLibs_Static$ $CppRTLibs_Static$'
            elif self.subsystem == 'windows':
                linker_options = ' /subsystem:windows /entry:WinMainCRTStartup $CRTLibs_Static$ $CppRTLibs_Static$'
            else:
                linker_options = ''

            if self.additional_libs:
                linker_options += " " + " ".join(self.additional_libs)
            writer.var_append('LinkerOptions', linker_options)

class DLL(Target):
    def __init__(self,
                 name: str,
                 deps=None):
        super(DLL, self).__init__(name, deps, True)

    def _write_function(self, writer: FBuildFileWriter):
        pass # TODO

class Copy(Target):
    def __init__(self, name: str, source: str, dest: str, deps:Optional[list[Target]]=None,
        build_config_dependent:bool=True):

        super(Copy, self).__init__(name, deps, build_config_dependent)

        self.source = source
        self.dest = dest

    def _write_function(self, writer: FBuildFileWriter):
        with writer.target('Copy', self.fbuild_name) as ctx:
            writer.var('Source', self.source)
            writer.var('Dest', self.dest)

class Alias(Target):
    def __init__(self, name: str, deps: Optional[list[Target]],
        build_config_dependent:bool=True):

        super(Alias, self).__init__(name, deps, build_config_dependent)

    def _write_function(self, writer: FBuildFileWriter):
        with writer.target('Alias', self.fbuild_name) as ctx:
            writer.var('Targets', [dep.fbuild_name for dep in self.deps])


import os
import sys
import subprocess
import argparse

def configure(args):
    current_path = os.getcwd()
    configure_file_path = os.path.join(current_path, "configure.py")
    if not os.path.exists(configure_file_path):
        print("Cannot find configure.py in directory!")
        exit(1)

    # Execute the configure script
    sys.path.insert(0, current_path)
    import configure

def build(args):
    # Run FastBuild build
    subprocess.run([".\\FastBuildSDK\\Binaries\\Windows\\FBuild.exe", args.target])

    # Create compilation database
    subprocess.run([".\\FastBuildSDK\\Binaries\\Windows\\FBuild.exe", "-compdb", args.target])

if __name__ == '__main__':
    parser_main = argparse.ArgumentParser(
        description = 'A Python build generator for FastBuild')
    subparsers = parser_main.add_subparsers()

    parser_configure = subparsers.add_parser('configure')
    parser_configure.set_defaults(func=configure)

    parser_build = subparsers.add_parser('build')
    parser_build.set_defaults(func=build)
    parser_build.add_argument('target')

    # parser_run = subparsers.add_parser('run')
    # parser_run.set_defaults(func=run)

    args = parser_main.parse_args()
    args.func(args)
