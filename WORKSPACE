load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")



new_git_repository(
    name = "gtest",
    remote = "https://github.com/google/googletest.git",
    commit = "a868e618c0607259c63f37d948b72586a13922ff",
    build_file_content = """
cc_library(
    name = "gtest",
    srcs = glob(
        [
            "google*/src/*.cc",
        ],
        exclude = glob([
            "google*/src/*-all.cc",
            "googlemock/src/gmock_main.cc",
        ]),
    ),
    hdrs = glob(["*/include/**/*.h"]),
    includes = [
        "googlemock/",
        "googlemock/include",
        "googletest/",
        "googletest/include",
    ],
    linkopts = ["-pthread"],
    textual_hdrs = ["googletest/src/gtest-internal-inl.h"],
    visibility = ["//visibility:public"],
    copts = [ "-DGTEST_USE_OWN_TR1_TUPLE=0" ],
)

cc_library(
    name = "gtest_main",
    srcs = [
        "googletest/src/gtest_main.cc",
    ],
    deps = [ ":gtest" ],
#    copts = [ "-DGTEST_HAS_TR1_TUPLE=0", "-DGTEST_USE_OWN_TR1_TUPLE=0"],
    visibility = ["//visibility:public"],
)
    """
)

