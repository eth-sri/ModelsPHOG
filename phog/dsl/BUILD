
cc_library(name = "dsl",
           srcs = [
               "tcond_language.cpp",
               "tcond_language.h",
               "simple_cond.cpp",
               "simple_cond.h",
               "branched_cond.cpp",
               "branched_cond.h",
               "tgen_program.cpp",
               "tgen_program.h",

#               "tree_dataset.cpp",
#               "tree_dataset.h",
           ],
           deps = [
               "//base",
               "//phog/tree",
           ],
           linkopts = ["-lm"],  #  Math library
           visibility = ["//visibility:public"])

cc_test(name = "tcond_language_test",
        srcs = ["tcond_language_test.cpp"],
        deps = [":dsl",
                "@gtest//:gtest",])

cc_test(name = "simple_cond_test",
        srcs = ["simple_cond_test.cpp"],
        deps = [":dsl",
                "@gtest//:gtest",])

cc_test(name = "branched_cond_test",
        srcs = ["branched_cond_test.cpp"],
        deps = [":dsl",
                "@gtest//:gtest",])

cc_test(name = "tgen_program_test",
        srcs = ["tgen_program_test.cpp"],
        deps = [":dsl",
                "@gtest//:gtest",])

#cc_test(name = "tree_dataset_test",
#        srcs = ["tree_dataset_test.cpp"],
#        deps = [":dsl",
#                "@gtest//:gtest",])

#cc_test(name = "tgen_model_test",
#        srcs = ["tgen_model_test.cpp"],
#        deps = [":dsl",
#                "@gtest//:gtest",])

