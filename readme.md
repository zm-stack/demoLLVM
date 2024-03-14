# ChaincodeChecker Instruction

本工具用于自动化检测Fabric链码中存在的常见漏洞，并允许用户通过特定语法自定义功能约束对于链码进行形式化验证。

[TOC]

## 功能介绍

- 链码漏洞检测功能，通过将链码转译成LLVM IR并通过污点分析进行漏洞检测，覆盖漏洞列表如下：

  |  序号  |  漏洞名称  |  说明  |
  |:------ |:----------|:-------|
  | 1 | 全局变量及字段声明  | 不同节点调用链码得到的全局变量和字段值可能不同|
  | 2 | 随机数与时间戳依赖  | 执行路径及结果不应依赖随机数和时间戳         |
  | 3 | range over map    | 遍历键值返回的顺序随机，可能会导致结果不确定  |
  | 4 | gorountine        | 链码中不鼓励使用并发                        |
  | 5 | 区块链引入的不确定性| web服务，系统命令，外部文件访问，第三方库    |
  | 6 | 幻影读             | 部分查询函数返回的结果不能用于更新账本       |
  | 7 | 跨通道链码调用     | 不能通过调用其他通道上的链码提交数据          |
  | 8 | 写后读            | 不能在同一个交易中更新并读取同一个key值       |
  | 9 | 隐私泄露          | 隐私数据不应影响执行路径，不应作为参数和返回值 |
  | 10 | 溢出             | 未受保护的数值计算                          |

- 链码形式化验证功能，通过将插入注释的链码转译为LLVM IR并借助IR形式化验证工具实现可满足性判定。

## 文件说明

- manual: 官方文档提供的操作提示
  - instance: 一些简单的PASS练手项目
- myPass: 自行编写的PASS，用于检测链码隐私泄露漏洞
- SBOX: myPass的相近工作，用于检测密码算法中的计时攻击

## 依赖

1. 环境依赖
  在Ubuntu上，建议安装的llvm-15和clang-15，可参照[clang安装步骤](https://askubuntu.com/questions/1415621/can-you-help-me-install-clang-14-on-ubuntu-18-04-i-think-i-need-a-valid-repo-as)和[gollvm项目](https://go.googlesource.com/gollvm)。后续会用到clang-15、opt-15等指令，请确保它们可正常调用（可能需要添加环境变量，如果有多个带版本后缀的可执行文件，可以考虑通过update-alternatives指令或使用ln指令来创建一个软链接/usr/bin/clang指向你所需要的版本的可执行文件。）

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
   clang-15 `llvm-config-15 --cxxflags` -fno-rtti -fPIC -shared Hello.cpp -o LLVMHello.so `llvm-config-15 --ldflags`
   ```

## 调用

- __输入__：LLVM IR文件（文件名后缀为.bc或.ll）  
- __输出__：检测出的漏洞信息。
- __调用指令__：  
- 调用PASS检测IR程序
    使用旧的pass的需要注意在命令中添加-enable-new-pm=0选项。
    添加-time-passes选项获取当前Pass和其他Pass的执行时间信息。

    ```bash
    opt-15 -load path/to/LLVMHello.so -hello main.bc -o /dev/null -enable-new-pm=0
    ```

    使用以下命令查看注册的Pass的情况

    ```bash
    opt-15 -load LLVMHello.so -help
    ```
