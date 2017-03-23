# Installation

## Install bazel (Google Build System)

Execute the following instructions to get Java8 installed

```
sudo add-apt-repository ppa:webupd8team/java
sudo apt-get update
sudo apt-get install oracle-java8-installer
```

Download bazel

```
echo "deb http://storage.googleapis.com/bazel-apt stable jdk1.8" | sudo tee /etc/apt/sources.list.d/bazel.list
curl https://storage.googleapis.com/bazel-apt/doc/apt-key.pub.gpg | sudo apt-key add -
```

Install bazel

```
sudo apt-get update
sudo apt-get install bazel
```

Get latest version of Bazel.
```
sudo apt-get upgrade bazel
```

For for information: [http://www.bazel.io/docs/install.html]

## Install prerequisites

```
sudo apt-get install libgoogle-glog-dev libgflags-dev libargtable2-dev cmake
```

# Compile


```
bazel build -c opt //...
```

In case you see an error, you may need to set java home by calling: export JAVA_HOME=/usr/lib/jvm/java-8-oracle

# Run tests

```
bazel test -c opt //...
```

You can also use -c dbg instead of -c opt to run with debug version of the code.
