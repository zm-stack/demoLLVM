# ChaincodeChecker Instruction

[TOC]

## 文件说明

- manual: 官方文档提供的操作提示
  - instance: 一些简单的PASS练手项目
- myPass: 自行编写的PASS，用于检测链码隐私泄露漏洞
- SBOX: myPass的相近工作，用于检测密码算法中的计时攻击

## 依赖

1. 环境依赖
在Ubuntu上，建议安装的llvm-10和clang-12级以上版本，可参照[clang安装步骤](https://askubuntu.com/questions/1415621/can-you-help-me-install-clang-14-on-ubuntu-18-04-i-think-i-need-a-valid-repo-as)和[gollvm项目](https://go.googlesource.com/gollvm)。（clang-12到clang-15存在bug，参见[链接](https://github.com/llvm/llvm-project/issues/59322)）
后续会用到clang-13、opt-13、llvm-config等指令，请确保它们可正常调用（可能需要添加环境变量）。如果有多个带版本后缀的可执行文件，可以考虑通过update-alternatives指令或使用ln指令来创建一个软链接/usr/bin/clang指向你所需要的版本的可执行文件。

2. go程序转llvm ir

   - 编译工具链接：gollvm <https://go.googlesource.com/gollvm>
   - dockerhub链接：<https://hub.docker.com/r/prodrelworks/gollvm-docker>
   - 注意：
     - 请不要安装golang的go软件包，否则可能会导致包管理错误。
     - 国内用户如果包拉取过程出现问题，请确认是否mod下载已换至国内源。
     - 建议通过gollvm源码进行安装，Docker用户可能会在转译链码过程中遇到go版本过旧的问题。

   从go源文件生成IR

   ```bash
   go build -work -x mypackage.go 1> transcript.txt 2>&1
   egrep '(WORK=|llvm-goc -c)' transcript.txt
   # 注意替换命令中的$WORK为具体路径，并将输出改为-o mypackage.ll -S -emit-llvm，如
   WORK=/tmp/go-build887931787
   /t/bin/llvm-goc -c -g -m64 -fdebug-prefix-map=$WORK=/tmp/go-build -gno-record-gcc-switches -fgo-pkgpath=command-line-arguments -fgo-relative-import-path=/mygopath/src/tmp -o $WORK/b001/_go_.o -I $WORK/b001/_importcfgroot_ ./mypackage.go
   # 被修改为如下形式
   # 优化等级选择O0，以尽可能保留源码信息
   /t/bin/llvm-goc -c -O0 -m64 -fdebug-prefix-map=/tmp/go-build887931787=/tmp/go-build -gno-record-gcc-switches -fgo-pkgpath=command-line-arguments -fgo-relative-import-path=/mygopath/src/tmp -o mypackage.ll -S -emit-llvm -I /tmp/go-build887931787/b001/_importcfgroot_ ./mypackage.go
   ```

3. 将PASS编译为.so文件

   - llvm-config提供CXXFLAGS与LDFLAGS参数方便查找LLVM的头文件与库文件。还可以用lvm-config --libs提供动态链接的LLVM库。
   - -fPIC -shared提供编译动态库的必要参数。
   - 因为LLVM没用到RTTI，所以用-fno-rtti 来让我们的Pass与之一致。

   ```bash
   clang-13 `llvm-config --cxxflags` -fno-rtti -fPIC -shared Hello.cpp -o LLVMHello.so `llvm-config --ldflags`
   ```

## 调用

- __输入__：LLVM IR文件（文件名后缀为.bc或.ll）  
- __输出__：检测出的漏洞信息。
- __调用指令__：  
- 调用PASS检测IR程序
    使用旧的pass的需要注意在命令中添加-enable-new-pm=0选项。
    添加-time-passes选项获取当前Pass和其他Pass的执行时间信息。

    ```bash
    opt-13 -load path/to/LLVMHello.so -hello main.bc -o /dev/null -enable-new-pm=0
    ```

    使用以下命令查看注册的Pass的情况

    ```bash
    opt -load LLVMHello.so -help
    ```

## 污点源

关于链码的操作可以分类为以下3类，API之前的数字是该API的编号：

- 读操作：
  - 12 GetPrivateData(collection string, key string) ([]byte, error)
  - 13 GetPrivateDataByPartialCompositeKey(collection, objectType string, attributes []string) StateQueryIteratorInterface, error)
  - 14 GetPrivateDataByRange(collection, startKey, endKey string) (StateQueryIteratorInterface, error)
  - 16 GetPrivateDataQueryResult(collection, query string) (StateQueryIteratorInterface, error)
- 写操作
  - 33 PutPrivateData(collection string, key string, value []byte) error
- 删操作
  - 2 DelPrivateData(collection string, key string) error
  - 32 PurgePrivateData(collection string, key string) error

1. 交易字段造成的隐私泄露
   |序号|泄露原因|污点|判定|检测|
   |:--|:--|:--|:--|:--|
   |1 |参数  |无  |存在写，不存在transient    |检查API是否存在于同一个函数 |
   |2 |分支  |get |存在更新，污点影响到Br语句  |定位API，定位分支语句      |
   |3 |返回值|get |存在更新，且污点影响到返回值 |定位API，定位返回值        |

2. 不必要的隐私扩散
   |序号|泄露原因|污点|判定|检测|
   |:--|:--|:--|:--|:--|
   |1 |全局变量 |get |污点影响到全局变量  |检查API是否存在于同一个函数 |
   |2 |其他链码 |get |污点传播到函数参数  |定位InvokeChaincode       |
