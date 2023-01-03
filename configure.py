from tools.fbuild import Project, Compiler, ExternalLibrary, HeaderOnlyLibrary, ObjectList, Executable, Copy, Alias

project = Project(name="flock3d", platform="windows")

lib_vulkan = ExternalLibrary(
    name="vulkan",
    basepath="C:/VulkanSDK/1.3.216.0",
    includes=["Include"],
    defines=["VULKAN_HPP_NO_EXCEPTIONS"])

compiler_glslc = Compiler(
    name="glslc",
    executable=f"{lib_vulkan.basepath}/Bin/glslc.exe",
    family="custom",
    allow_distribution=False)

project.add_compiler(compiler_glslc)

glsl_shaders = ObjectList(
    name="glsl_shaders",
    basepath="src/shaders",
    compiler=compiler_glslc,
    compiler_options='"%1" -o "%2"',
    source_path=".",
    source_patterns=["*.vert", "*.frag", "*.comp"],
    dest_path="assets/shaders",
    dest_extension=".spv",
    dest_keep_base_extension=True,
    build_config_dependent=False
)

lib_sdl = ExternalLibrary(
    name="sdl",
    basepath="deps/SDL2-2.0.22",
    includes=["include"],
    libs=["lib/x64/SDL2.lib", "lib/x64/SDL2main.lib"])

lib_freetype = ExternalLibrary(
    name="freetype",
    basepath="deps/freetype-2.12.1",
    includes=["include"],
    libs=["lib/freetype.lib"])

lib_linavg = ObjectList(
    name="linavg",
    basepath="deps/LinaVG-1.1.3",
    source_files=[
        "src/Core/Backend.cpp",
        "src/Core/Common.cpp",
        "src/Core/Drawer.cpp",
        "src/Core/Math.cpp",
        "src/Core/Renderer.cpp",
        "src/Core/Text.cpp",
        "src/Utility/Utility.cpp",
    ],
    includes=["include"],
    defines=["LINAVG_TEXT_SUPPORT"],
    deps=[lib_freetype])

lib_volk = ObjectList(
    name="volk",
    basepath="deps/volk-sdk-1.3.216",
    source_files=["volk.c"],
    includes=["."],
    defines=["VOLK_USE_PLATFORM_WIN32_KHR"],
    deps=[lib_vulkan])

lib_fmt = ObjectList(
    name="fmt",
    basepath="deps/fmt-9.0.0",
    source_files=["src/format.cc", "src/os.cc"],
    includes=["include"],
    defines=["FMT_EXCEPTIONS=0"])

lib_glm = ObjectList(
    name="glm",
    basepath="deps/glm-0.9.9.8",
    source_files=["glm/detail/glm.cpp"],
    includes=["."])

lib_imgui = ObjectList(
    name="imgui",
    basepath="deps/imgui-1.87",
    source_files=["imgui.cpp", "imgui_demo.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp",
             "backends/imgui_impl_sdl.cpp", "backends/imgui_impl_vulkan.cpp"],
    includes=[".", "backends"],
    defines=["IMGUI_IMPL_VULKAN_NO_PROTOTYPES"],
    deps=[lib_sdl, lib_volk])

lib_doctest = HeaderOnlyLibrary(
    name="doctest",
    basepath="deps/doctest-2.4.9",
    includes=["."],
    defines=["DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS"])

lib_parallel_hashmap = HeaderOnlyLibrary(
    name="parallel_hashmap",
    basepath="deps/parallel-hashmap-1.34",
    includes=["."])

lib_physfs = ObjectList(
    name="physfs",
    basepath="deps/physfs-3.0.2",
    compile_as_c=True,
    source_files=["src/physfs.c", "src/physfs_unicode.c", "src/physfs_byteorder.c",
        "src/physfs_archiver_dir.c", "src/physfs_archiver_zip.c", "src/physfs_platform_windows.c"],
    includes=["src"],
    defines=["PHYSFS_SUPPORTS_DEFAULT=0", "PHYSFS_SUPPORT_ZIP"])

lib_tinyobjloader = ObjectList(
    name="tinyobjloader",
    basepath="deps/tinyobjloader-2.0.0rc9",
    source_files=["tiny_obj_loader.cc"],
    includes=["."])

lib_cgltf = ObjectList(
    name="cgltf",
    basepath="deps/cgltf-1.12",
    source_files=["cgltf.c"],
    includes=["."])

lib_nanothread = ObjectList(
    name="nanothread",
    basepath="deps/nanothread-master",
    source_files=["src/nanothread.cpp", "src/queue.cpp"],
    priv_includes=["src"],
    includes=["include"],
    compiler_options=" -mcx16")

lib_tracy = ObjectList(
    name="tracy",
    basepath="deps/tracy-0.9",
    source_files=["TracyClient.cpp"],
    defines=["TRACY_ENABLE"],
    includes=["."])

lib_gen_arena = HeaderOnlyLibrary(
    name="gen_arena",
    basepath="deps/gen-arena-0.0.1",
    includes=["."])

lib_engine = ObjectList(
    name="engine",
    basepath="engine",
    source_files=[
        "engine.cpp",
        "input.cpp",
        "mesh.cpp",
        "model_loader.cpp",
        "res.cpp",
        "terrain.cpp",
        "render/renderer.cpp",
        "render/mesh_renderer.cpp",
        "render/imgui_renderer.cpp",
        "render/im3d_renderer.cpp",
        "render/wireframe_renderer.cpp",
        "render/linavg_renderer.cpp",
        "systems/controls.cpp",
        "systems/observer.cpp",
        "systems/player.cpp",
        "systems/boid.cpp",
        "core/log.cpp",
        "core/file.cpp",
        "core/random.cpp",
        "core/win32_utils.cpp",
        "terrain_algo.cpp",
        "vk_mem_alloc.cpp",
        "stb/stb.c"
    ],
    includes=["."],
    deps=[lib_volk, lib_sdl, lib_freetype, lib_linavg, 
        lib_fmt, lib_glm, lib_imgui, lib_parallel_hashmap, lib_physfs, lib_cgltf, 
        lib_nanothread, lib_tracy, lib_gen_arena],
)

linked_win32_libs = ["shell32.lib", "kernel32.lib", "advapi32.lib", "user32.lib", 
        "dbghelp.lib", "ws2_32.lib", # for Tracy
        "imm32.lib"] # for imgui

lib_flock3d = ObjectList(
    name="flock3d_lib",
    basepath="engine",
    source_files=["flock3d.cpp"],
    deps=[lib_engine])

exe_flock3d = Executable(
    name="flock3d_exe",
    dest=f"{project.binary_path}/flock3d.exe",
    deps=[lib_flock3d],
    subsystem='windows',
    additional_libs=linked_win32_libs
)

lib_linavg_test = ObjectList(
    name="linavg_test_lib",
    basepath="engine",
    source_files=["linavg_test.cpp", "linavg_demo_screens.cpp"],
    deps=[lib_engine])

exe_linavg_test = Executable(
    name="linavg_test_exe",
    dest=f"{project.binary_path}/linavg_test.exe",
    deps=[lib_linavg_test],
    subsystem='windows',
    additional_libs=linked_win32_libs)

lib_test_ecs = ObjectList(
    name="test_ecs_lib",
    basepath="src",
    source_files=[
        "test_ecs.cpp",
        "core/log.cpp"
    ],
    includes=["."],
    defines=["OVERRIDE_ECS_COMPONENTS"],
    deps=[lib_fmt, lib_doctest, lib_gen_arena]
)

exe_test_ecs = Executable(
    name="test_ecs_exe",
    dest=f"{project.binary_path}/test_ecs.exe",
    deps=[lib_test_ecs],
    subsystem='console',
    additional_libs=['kernel32.lib']
)

copy_sdl2_dll = Copy(
    name="copy_sdl2_dll",
    source=f"{lib_sdl.basepath}/lib/x64/SDL2.dll",
    dest=f"{project.binary_path}/SDL2.dll",
)

copy_freetype_dll = Copy(
    name="copy_freetype_dll",
    source=f"{lib_freetype.basepath}/lib/freetype.dll",
    dest=f"{project.binary_path}/freetype.dll")

alias_flock3d = Alias(
    name="flock3d",
    deps=[exe_flock3d, copy_sdl2_dll, copy_freetype_dll, glsl_shaders]
)

alias_linavg_test = Alias(
    name="linavg_test",
    deps=[exe_linavg_test, copy_sdl2_dll, copy_freetype_dll, glsl_shaders])

alias_tests = Alias(
    name="tests",
    deps=[exe_test_ecs]
)

project.add_targets([alias_flock3d, alias_linavg_test, alias_tests])

project.generate()