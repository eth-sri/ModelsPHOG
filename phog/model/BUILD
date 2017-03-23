
cc_library(name = "model",
           srcs = [
               "model.cpp",
               "model.h",
           ],
           deps = [
               "//base",
               "//phog/dsl",
               "//phog/tree",
           ],
           linkopts = ["-lm"],  #  Math library
           visibility = ["//visibility:public"])

cc_binary(name = "evaluate",
          srcs = [ "evaluate.cpp" ],
          deps = [ "//base",
                   ":model",
                 ])
