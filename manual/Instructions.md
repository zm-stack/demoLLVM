# 终端指令（Terminator Instructions）
所有基本块的结束指令，都是终端指令。

## ret指令
+ ret < type> < value>  
Return a value from a non-void function, must be of first-class type  
+ ret void  
Return from void function  
当执行ret指令时，控制流程将返回到调用函数的上下文。如果是通过call指令调用，则在指令调用后继续执行。如果是通过invoke指令调用，则在normal目标块(destination block)的开头继续执行。如果指令返回一个值，该值将设置为call或者invoke指令的返回值。

## br指令
+ br i1 < cond>, label < iftrue>, label < iffalse>  
条件跳转
+ br label < dest>  
无条件跳转

## switch指令
+ switch < intty> < value>, label < defaultdest> [ < intty> < val>, label < dest> ... ]  
< intty> < value>是一个整型值，决定着具体跳到要哪个分支。  
label < defaultdest>这是默认的label，如果分支中无匹配，就跳到这个分支。  

## indirectbr指令
+ indirectbr < somety>* < address>, [ label < dest1>, label < dest2>, ... ]  
跳转到指定地址的基本块中，指定的地址必须是基本块地址(Addresses of Basic Blocks)常量。所有可能的目标块必须在标签列表中列出，否则此指令具有未定义行为。（br指令仅局限于函数内部块之间的跳转，不能跨函数跳转，而indirectbr指令可以实现跨函数块之间的跳转。）
< somety>* < address>是要跳转的地址。  
label < dest1>, label < dest2>, ... 上面地址指向的全部可能。

# 一元运算(Unary Operations)
## fneg指令
+ < result> = fneg [fast-math flags]* < ty> < op1>  
< ty> < op1>是操作数类型，必须是浮点类型或者浮点向量。  
fast-math flags是优化提示，以启用不安全的浮点(floating-point)优化。  
返回操作数符号位翻转后的一个副本，也就是一个相反数。

# 二元运算(Binary Operations)
二元运算的两个操作数必须是相同类型，且返回的结果与操作数类型相同。

## add指令，sub指令，mul指令
< result> = add < ty> < p1>, < op2>  
< result> = add nuw < ty> < op1>, < op2>  
< result> = add nsw < ty> < op1>, < op2>  
< result> = add nuw nsw < ty> < op1>, < op2>  
< ty> < op1>, < op2>两个操作数的类型相同且必须是整型类型或者整型向量。  
nuw nsw，nuw和nsw分别代表No Unsigned Wrap和No Signed Wrap。如果nuw或nsw关键字存在，如果分别发生无符号溢出或有符号溢出，则指令的的结果值是poison value。

## fadd指令，fsub指令，fmul指令，fdiv指令
< result> = fadd [fast-math flags]* < ty> < op1>, < op2>  
返回两个浮点值的和。

## udiv指令，sdiv指令，
< result> = udiv < ty> < op1>, < op2>  
< result> = udiv exact < ty> < op1>, < op2>  
< ty> < op1>, < op2>两个操作数的类型相同且必须是整型类型或者整型向量，且第二个操作数的值不能为零，如果是整型向量，则第二个向量任一元素都不能为零。  
exact 关键字出现，但是op1不是op2的倍数(这个等式成立就是倍数((a udiv exact b) mul b) == a)，则产生的结果就是一个poison value。

## urem指令，srem指令，frem指令
两个操作数进行取余操作。

# 二进制位运算(Bitwise Binary Operations)
## shl指令，lshr指令
用法同add命令。左移运算符。
op2作为无符号值来处理，将op2向左按位移动opt1位。如果opt2的位数大于或等于opt1的位数，该指令将返回一个poison value。如果op1是向量，op2将会按照op1的元素逐个移动。  
nuw关键字出现，移出任何非零位，都会产生一个poison value。  
nsw 关键字出现，移出任何与结果的符号位不一致的位，都会产生一个poison value。

## ashr指令
算数右移运算符。（算术右移在最高位要补符号位的，而逻辑右移最高位都是零。）

## and指令，or指令，xor指令
< result> = and < ty> < op1>, < op2>  
按位逻辑与操作。两个操作数的类型相同且必须是整型类型或者整型向量。

# 向量运算(Vector Operations)
## extractelement指令
< result> = extractelement < n x < ty>> < val>, < ty2> < dx>  ; yields < ty>  
< result> = extractelement < vscale x n x < ty>> < val>, < ty2> < idx>  
从指定的向量中提取单个标量元素，有两个操作数，一个是向量类型；另一个是向量索引（索引值大于等于零，小于向量的长度）。如果索引值不在向量的长度范围内，会返回一个poison value。

## shufflevector指令
< result> = shufflevector < n x < ty>> < v1>, < n x < ty>> < v2>, < m x i32> < mask>  
< result> = shufflevector < vscale x n x < ty>> < v1>, < vscale x n x < ty>> v2, < vscale x m x i32> < mask>  
该指令总共有三个操作数，从前两个向量中抽取指定的一些元素，按照第三个shuffle mask构成一个新的向量，新向量的类型与被抽取的向量相同，长度与shuffle mask相同。  
< n x < ty>> < v1>, < n x < ty>> < v2>两个输入向量，如果输入向量中未定义的元素被选中，则新向量对应的元素也是未定义。< m x i32> < mask>(shuffle mask)，如果调整掩码未定义，则新向量未定义。

# 聚合运算(Aggregate Operations)
## extractvalue指令
< result> = extractvalue < aggregate type> < val>, < idx>{, < idx>}*  
用于在聚合数据中提取指定索引处的字段。  
< aggregate type> < val>聚合类型，数组或结构体。  
< idx>{, < idx>}*常量索引值，提取指定索引处的元素。

## insertvalue指令
< result> = insertvalue < aggregate type> < val>, < ty> < elt>, < idx>{, < idx>}*  
用于在聚合数据的指定索引处插入元素。  
< aggregate type> < val>聚合类型，数组或结构体。  
< ty> < elt>要插入的数据，< idx>{, < idx>}*常量索引值，提取指定索引处的元素。  

# 内存访问和寻址(Memory Access and Addressing Operations)
## alloca指令
< result> = alloca [inalloca] < type> [, < ty> < NumElements>] [, align < alignment>] [, addrspace(< num>)]  
< type>指令返回的指针类型，可以是任何类型。  
< ty> < NumElements>要分配的内存数量，如果没有指定，默认情况下该值为1。  
align < alignment>内存对齐值。如果指定了对齐值，对齐值不能大于1 << 29，且保证分配的值结果至少与该边界对齐；如果没有指定或者对齐值为零，目标可以选择在任何与类型兼容的范围内对齐分配。

## store指令
store [volatile] < ty> < value>, < ty>* < pointer>[, align <alignment>][, !nontemporal !<index>][, !invariant.group !< index>]  
store atomic [volatile] < ty> < value>, < ty>* < pointer> [syncscope("<target-scope>")] < ordering>, align < alignment> [, !invariant.group !< index>]  
< ty> < value>要存入的值，值的类型必须是已知长度的first class type。  
< ty>* < pointer>是一个指针，指向一块存值的地址。  
用于将值写入到指定的内存中。如果写入的值是标量类型(scalar type)，则值占用的字节数不能超越容纳类型所需要的最小字节数。

## load指令
<< result> = load [volatile] < ty>, < ty>* < pointer>[, align <alignment>][, !nontemporal !<index>][, !invariant.load !<index>][, !invariant.group !<index>][, !nonnull !<index>][, !dereferenceable !<deref_bytes_node>][, !dereferenceable_or_null !<deref_bytes_node>][, !align !<align_node>]  
< result> = load atomic [volatile] < ty>, < ty>* < pointer> [syncscope("<target-scope>")] < ordering>, align < alignment> [, !invariant.group !< index>]  
!< index> = !{ i32 1 }  
!< deref_bytes_node> = !{i64 < dereferenceable_bytes>}   
!< align_node> = !{ i64 < value_alignment> }  
< result>是一个变量，变量的值是从内存中加载出来的值。  
load < ty>加载类型，也就是前面变量的类型。  
< ty>* < pointer>是一个指针，指向要加载的内存，指针的类型必须是不包含不透明结构(opaque structural type)的first class type类型。  
用于从指定的内存中读取一个值，也可以说加载一个值。如果读取的值是一个标量类型(scalar type )，则读取值的字节数不能超越容纳类型所需要的最小字节数。

## getelementptr指令
< result> = getelementptr < ty>, < ty>* < ptrval>{, [inrange] < ty> < idx>}*  
< result> = getelementptr inbounds < ty>, < ty>* < ptrval>{, [inrange] < ty> < idx>}*  
< result> = getelementptr < ty>, < ptr vector> < ptrval>, [inrange] < vector index type> < idx>   
第一个< ty>是第一个索引< idx>使用的基本类型  
第二个< ty>*表示其后的基地址ptrval的类型。  
< ty> < idx>是第一组索引的类型和值， 可以出现多次，其后出现就是第二组、第三组等等索引的类型和值。（索引的类型一般为i32或i64，而索引使用的基本类型确定的是增加索引值时指针的偏移量。）  
获得指向数组的元素和指向结构体成员的指针，只执行地址计算，不访问内存，也可用于计算此类地址的向量。

# 转换操作(Conversion Operations)
## trunc…to…指令
< result> = trunc < ty> < value> to < ty2>  
截取目标值的高位，使剩余的部分转换成目标类型。

# 其他运算(Other Operations)
## icmp和fcmp指令
icmp指令的操作数类型是整型或整型向量、指针或指针向量。对于指针或指针向量，在做比较运算的时候，都会将其指向的地址值作为整型数值去比较  
fcmp指令要求操作数是浮点值或者浮点向量

## phi指令
< result> = phi [fast-math-flags] < ty> [ < val0>, < label0>], ...  
根据在当前block之前执行的是哪一个predecessor(前导) block来得到相应的值。  
phi 指令必须在basic block的最前面，也就是在一个basic block中，在phi 指令之前不允许有非phi指令，但是可以有多个phi指令。（phi指令的目的是为了实现SSA（静态单一赋值））  

## select指令
< result> = select [fast-math flags] selty < cond>, < ty> < val1>, < ty> < val2>   
根据条件选择一个值。  
selty < cond>是一个i1类型的值，如果这个值是1(true)，选择后面第一个参数，否则选择第二个参数。
< ty> < val1>, < ty> < val2>这两个参数是要被选择的值，两个参数的类型要相同，且都为first class type。

## call指令
< result> = [tail | musttail | notail ] call [fast-math flags] [cconv] [ret attrs] [addrspace(<num>)]< ty>|< fnty> < fnptrval>(< function args>) [fn attrs] [ operand bundles ]  
用于调用一个函数。该指令使控制流转移到指定的函数，其入参值绑定到被调函数的形参。在被调用函数中执行ret指令后，控制流程将会回到该指令上，并且被调函数的返回值将绑定到结果参数。
+ tail和musttail标记指示优化器应执行尾（tail）调用优化。tail标记是可以忽略的提示。musttail标记意味着该调用必须经过尾（tail）调用优化，以使程序正确。
+ notail标记表示优化器不应该向调用添加tail或musttail标记，它用于防止对调用执行尾调用优化。
+ fast-math flags仅对返回浮点标量或向量类型、浮点标量或向量数组（嵌套到任何深度）类型的调用有效，是用于启用不安全的浮点优化的优化提示。
+ cconv标记指示调用应该使用哪种调用约定，如果没有指定，调用默认使用C调用约定。该调用约定必须匹配目标函数的调用约定，否则行为是未定义的。
+ ret attrs列出返回值的参数属性，这里只有zeroext、signext和inreg属性有效。
+ addrspace(< num>)属性可用于指示被调用函数的地址空间，如果未指定，将使用data layout字符串中的程序地址空间。
+ < ty>是调用指令本身的类型，也是返回值的类型，没有返回值的函数被标记为void。
+ < fnty>是被调用函数的签名。参数类型必须匹配此签名所隐含的类型，如果函数不是可变参数，则可以省略此类型。
+ fnptrval是一个LLVM值，包含要调用的函数的指针（定义一个函数时的函数名）。在大多数情况下，这是一个直接的函数调用，但是间接调用则尽可能调用任意指向函数值的指针。
+ function args是函数参数列表，有参数类型和参数属性，所有的参数都是first class type。如果函数签名表明函数接受可变数量的参数，则可以指定额外的参数。
+ fn attrs函数属性。
+ operand bundles操作数集。