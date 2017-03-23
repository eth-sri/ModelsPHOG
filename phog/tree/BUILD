
cc_library(name = "tree",
           srcs = [
               "pbox.cpp",
               "pbox.h",
               "tree.cpp",
               "tree.h",
               "tree_index.cpp",
               "tree_index.h",
               "tree_slice.h",
           ],
           deps = [
               "//base",
               "//json",
               "//base/sparsehash"
           ],
           visibility = ["//visibility:public"])

cc_test(name = "pbox_test",
        srcs = ["pbox_test.cpp"],
        deps = [":tree",
                "@gtest//:gtest"])

cc_test(name = "tree_index_test",
        srcs = ["tree_index_test.cpp"],
        deps = [":tree",
                "@gtest//:gtest",])

cc_test(name = "tree_test",
        srcs = ["tree_test.cpp"],
        deps = [":tree",
                "@gtest//:gtest"])
