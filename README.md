php-xsplit
==========

xsplit是一个PHP扩展，提供基于MMSEG算法的分词功能
* 原项目地址：
https://code.google.com/p/xsplit/
* license GNU GPL v2

* 本地址修正了一些编译上的invalid conversion from 'const char*' to 'char*' 之类的错误:
* 目前在 gcc 版本 4.7.2 PHP 5.4.13 上编译通过


xsplit是一个PHP扩展，提供基于MMSEG算法的分词功能。目前只在linux下测试并部署过，希望有朋友可以帮忙编译提供windows下的dll。

xsplit主要有以下几个函数：

```
bool xs_build ( array $words, string $dict_file )

resource xs_open (string $dict_file [, bool $persistent])

array xs_split ( string $text [, int $split_method = 1 ［, resource $dictionary_identifier ］ ] )

mixed xs_search ( string $text ［, int $search_method [, $dictionary_identifer ] ］ )

string xs_simhash( array $tokens [, bool $rawoutput] )

int xs_hdist（ string $simhash1, $string $simhash2)

```

安装过程与一般的PHP扩展安装一样

```
$phpize
$./configure --with-php-config=/path/to/php-config
$make
$make install
```

php.ini中可以设置以下参数：

```
xsplit.allow_persisten = On

xsplit.max_dicts = 5

xsplit.max_persistent = 3

xsplit.default_dict_file = /home/xdict

xsplit.allow_persistent 是否允许加载持久词典

xsplit.max_dicts 允许同时打开的最大词典数目

xsplit.max_persistent 允许同时打开的最大持久词典数目

xsplit.default_dict_file 默认的词典，没有指定词典时会调用此词典
```

源码中有一个utils目录，包含

``
make_dict.php 提供命令行方式创建词典

xsplit.php 一个简单的示例文件

xdict_example.txt 一个文本词库的格式示例
```

make_dict.php的使用例子如下：

```
$php make_dict.php ./xdict_example.txt ./xdict.db
```

文本词库的格式请参考xdict_example.txt

```
bool xs_build (array $words, string $dict_file)
```

从$words数组建立名称为$dict_file的词典，若成功则返回true。$words数组的格式请参考示例，key为词语，value为词频。

```
string xs_simhash( array $tokens [, bool $rawoutput] )
```

计算simhash。由于

例子如下：

```php
<?php
$dict_file='dict.db';

$dwords['美丽']=100;

$dwords['蝴蝶']=100;

$dwords['永远']=100;

$dwords['心中']=100;

$dwords['翩翩']=100;

$dwords['飞舞']=100;

$dwords['翩翩飞舞']=10;

if(!xs_build($dwords, $dict_file)) {

    die('建立词典失败！');
    
}
```
```
resource xs_open (string $dict_file bool $persistent?)
```

打开一个词典文件，并返回一个resource类型的identifier。$persistent可以指定是否是持久化词典，持久化词典在这里可以理解为词典资源生命周期的不同，一般情况下$persistent=true或者默认缺省即可。在进行分词的时候，可以指定不同的词典。

```php
$dict_file_1 = 'xdcit.db';

$dict_file_2 = 'mydict.db';

$dict1 = xs_open($dict_file);

xs_open($dict_file); 
```

```
array xs_split ( string $text int $split_method = 1 ［, resource $dictionary_identifier ］ ? )
```

对文本进行分词，可以指定分词方法和词典。分词方法目前有两种，一个是MMSEG算法（默认），一个是正向最大匹配，分别用常量 XS_SPLIT_MMSEG和XS_SPLIT_MMFWD表示。返回值是一个数组，包含所有切分好的词语。如果不指定词典，最后一次打开的词典将被使用。

```php
<?php
$text="那只美丽的蝴蝶永远在我心中翩翩飞舞着。";
$dict_file = 'xdict.db';
$dict_res = xs_open($dict_file);
$words = xs_split($text);  /* 此处没有指定词典资源，默认使用最后一次打开的词典 */

$words1 = xs_split($text, XS_SPLIT_MMSEG, $dict_res);

mixed xs_search ( string $text ［, int $search_method $dictionary_identifer ? ］ ) 基于双数组trie树提供的一些功能，$search_method有四个常量表示：

XS_SEARCH_CP : darts的commonPrefixSearch封装，如果没有找到，返回false。

XS_SEARCH_EM : darts的exactMatchSearch封装，如果没有找到，返回false。

XS_SEARCH_ALL_SIMPLE : 按照词典返回所有词语词频总和，一个INT型数值。

XS_SEARCH_ALL_DETAIL : 按照词典返回所有词典的词频，并以数组形式返回每一个词语的详细统计。

XS_SEARCH_ALL_INDICT : 返回词典里的词语，可以去掉标点、特殊符号之类的，但是连续的数字和字母会默认自动返回（目前采用MMSEG算法，hard coding的，有需要可以改源码）。
```

如果不指定词典，最后一次打开的词典将被使用。

```php
<?php
xs_open($dict_file);
$text="那只美丽的蝴蝶永远在我心中翩翩飞舞着。";
$word='翩翩飞舞';
$result=xs_search($word, XS_SEARCH_CP); /* common prefix search */
var_dump($result);
$result=xs_search($word, XS_SEARCH_EM); /* exact match search */
var_dump($result);
$result=xs_search($text, XS_SEARCH_ALL_SIMPLE);
var_dump($result);
$result=xs_search($text, XS_SEARCH_ALL_DETAIL);
var_dump($result);
```

```
string xs_simhash( array $tokens [, bool $rawoutput] )
```

计算simhash。这里所有token权重都是1，$tokens的例子如array('在', '这个', '世界')。$rawoput默认为0，即返回simhash的hex string形式，如md5， sha1函数一样；如过$rawoput为真，返回一个8字节的字符串，这个字符串实际上是一个64 bits的整型数，uint64_t，在一些特殊情况下可以用到。

```
int xs_hdist（ string $simhash1, $string $simhash2)
```

计算汉明距离。

```php
<?php
xs_open('xdict');
$text1="那只美丽的蝴蝶永远在我心中翩翩飞舞着。";
$text2="那只美丽的蝴蝶永远在我心中翩翩飞舞。";
$tokens1=xs_search($text1, XS_SEARCH_ALL_INDICT); /* 去掉标点等特殊符号，经过实验，计算simhash时，一些标点、换行、特殊符号等对效果影响较大 */
$tokens2=xs_search($text2, XS_SEARCH_ALL_INDICT);

$simhash1=xs_simhash($tokens1);
$simhash2=xs_simhash($tokens2);

echo "simhash1 is {$simhash1}\n";
echo "simhash2 is {$simhash2}\n";

$hamming_dist=xs_hdist($simhash1, $simhash2);

echo "bit-wise format:\n";
echo decbin(hexdec($simhash1)), "\n";
echo decbin(hexdec($simhash2)), "\n";

echo "hamming distance is {$hamming_dist}\n";
```
