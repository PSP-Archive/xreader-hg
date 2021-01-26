xReader是PSP专用读书和看图软件，支持任意大小点阵字体，支持html读取，编码转换，竖看，翻页保留行，书签及自动书签等，可以看zip和chm和rar内的文本文件，有自动降频节能(不播放mp3时)。
看图支持bmp, tga, jpg,png和gif，支持较大的图，采用两次插值和三次立方采样缩放，保证图像柔和度不失真(测试了起点的VIP图片，700*3xxx大小，缩放到适应屏幕宽，效果完美，由于缩放的算法复杂，显示点也多，所以没有降频)
完美解决文件名中文问题和长文件名问题
支持mp3播放
以上介绍纯属抄袭eReader 1.61介绍。
其实xReader是一个eReader的分枝版本。开发的本意是开发一个能在在PSP2000，4GMS卡上正常工作的eReader。
然而eReader已经停止开发。所以xReader从eReader中分裂出来成为了一个独立的软件。

xReader对eReader的改进：

1. 对eReader的兼容性：
xReader完全兼容eReader所用的字体，UI和操作方式等。 
2. 更好的稳定性：
xReader花大力气改进了eReader存在的缓冲溢出问题，修复了许多会导致eReader死机的BUG。
3. 更好的功能支持：
xReader支持自动翻页和自动滚屏功能，支持读取图像EXIF信息，支持解压缩档案文件，支持自动休眠
等等eReader不支持的功能。
4. 更好的硬件支持:
xReader支持8GMS卡，支持PSP2000（以及其64MB内存），支持最新M33版本。
5. 支持多种音乐格式
xReader支持播放MP3（硬/软解码）、Musepack(MPC)、TTA、FLAC、APE、WAV格式及其标签等信息显示

未来展望：
将支持Graphics Engine绘图
将支持LRC歌词自动搜索下载
将重构代码以便代码复用

xReader电子书目录生成工具: GenIndex
使用方法：
1. 在你的TXT电子书里加入<<<<符号标注每章开始等
如
<<<<第一章 XXX
....
<<<<第二章 YYY
2. 保存后，将TXT拖到GenIndex图标中，GenIndex就会为你的电子书生成
一个简便的目录：
===================
页数 目录
1    第一章 XXX
2    第二章 YYY
===================
3. 页数数字就是GI值，看书时在信息栏里有显示，如果你要移动到某章，翻页直到GI相等即可找到该章。GI是绝对页码，不会因为字体大小而改变。
注：GenIndex不能处理UTF8 TXT文件，请先转换为GBK编码

常见问题:

中文文件无法打开：
在PSP XMB菜单中设定中《主机设定》、《文字设定》
设置为Simpleified Chinese GBK(936)即可。

有关自动换行功能:
设置自动翻页模式为自动翻页后
自动翻页时延决定多快翻一次页,单位为秒.如果被关闭,将不会自动翻页

设置自动翻页模式为自动换行后:
自动换行步进(由于某种原因要重新开菜单才能看到)决定一次换行步进多少像素
自动换行时延决定多快换一次行,单位约为1/30秒.如果被关闭,还会自动换行
比如设置为自动翻页模式模式为自动换行,自动换行步进为1,自动换行时延为1时,是平滑滚动
设置为自动翻页模式模式为自动换行,自动换行步进为12,自动换行时延为30时,是约一秒换一行
由于PSP LCD硬件限制，请在自动滚动时把前景色和背景色设置的尽量相似，以消除抖动感。

有关自动休眠：
自动休眠在“系统选项”中“自动休眠”设置一个分钟数（一般可设置为30）。如果在这样长的时间
内不按任何键，PSP就会自动关机。对夜猫子们十分有用。

有关编码切换:
xReader中在mp3信息条内按左+三角键切换歌词编码,原ID3编码切换改为右+三角键

解压压缩包里的文件:
在压缩包文件列表中用方块键选择欲解压的文件，按三角键，再按L键复制。
退回到文件目的目录里，按三角键，再按START键粘贴。
注意：目前无法创建中文文件名，文件名中所有的汉字会被截去，后面按上文件大小的数字。
如：“你好世界abc.txt”会被转换为“abc_12345.txt”。

有关重新编排文本：
自动换行的TXT给较小显示空间的PSP带来显示上的极大麻烦。
如果字体较大，一行显示不完。
开启了自动编排文本后，就能自动将换行和段落开始分开了。

重启xReader
退出xReader时按着左耳朵键，xReader将重新启动。

手动输入GI
在阅读按键中设置手动输入GI翻页键，可以用它手动输入GI页码进行翻页。
GI页码是由GenIndex程序生成的目录页码数字。

强制使用文字模式打开文件
选中文件，按三角键，再按方块键即可强制使用用TXT方式打开文件

有关xr_rdriver.prx
这个插件使xReader拥有启动其它自制软件、游戏，并在退出时自动返回xReader的功能。
它有以下要求：
游戏启动方式只支持March33、Sony NP9660、和Normal三种
xReader主程序必须放在K:\PSP\GAME\xReader目录
xr_rdriver.prx插件必须放在K:\seplugins\目录，但不用再加入到GAME.txt
某些自制软件和游戏可能会出现问题

有关打开带有密码的ZIP、RAR档案
内存小的用户最好不要使用这个功能。xReader会将密码保存到password.lst（明文）文件里（如果系统选项：保存档案密码为是）。出于安全考虑可以将其设置为否。一旦密码被保存便不需要再重复输入。

有关返回先前文件
在系统按键中可以设置“返回先前文件”系统热键。在文件选择中列表按该热键，可返回到先前打开的文本、图像等内容。

有关翻页模式：先左右、上下滚动
设置为这两种模式后，xReader浏览图像时，按上/下一张图将滚动显示图像。翻页滚动间隔决定图像滚动时先滚动多少秒，然后暂停多少秒。如果设置为零则不暂停。翻页滚动速度决定了图像滚动的速度。由于PSP LCD的硬件限制，一直滚动会生成残影。所以建议要设置滚动间隔。如果同时开启了幻灯播放和先左右、上下滚动，图像将按翻页设置进行滚动。

部分ini配置解释：
global:brightness:
PSP屏幕亮度（0-100）

global:hide_flash
是否隐藏Flash0、Flash1

image:no_repeat
是否在最后一张图像显示完成时退回文件列表

text:cfont_antialias
text:efont_antialias
分别决定是否使用中文、英文TTF字体反锯齿

text:cfont_cleartype
text:efont_cleartype
分别决定是否使用中文、英文TTF字体ClearType效果

text:cfont_embolden
text:efont_embolden
分别决定是否使用中文、英文TTF字体加粗效果

text:infobar_use_ttf_mode
阅读文本时是否使用TTF字体显示信息栏

music::apetagorder
APETagV2是否优先于ID3TagV2读取

text:ttf_ttf_load_to_memory
阅读文本时是否将TTF字体装载到内存，PSP-2000用户可设置为"true"，PSP-1000用户最好设置为"false"

text:infobar_style
当为0时信息栏为透明矩形，当为1时为分割直线

text:scroll_width
滚动条宽度，以像素记

global:save_password
是否保存ZIP、RAR档案密码到password.lst

text:hide_last_row
文本显示时是否隐藏最后一行，设置为真可解决分行问题

text:infobar_show_timer
文件信息栏是否显示时间电量信息

text:englishtruncate
开启英文换行截断单词功能，默认为打开

image:image_scroll_chgn_speed
开启图像翻页方式中上下、左右滚动时滚动时速度逐渐变化功能，默认为打开

text:ttf_haste_up
开启绘制TTF字型时升高CPU频率功能，可能会导致字体闪烁显示，默认为打开

text:linenum_style
如果设置为真，文本进度显示将使用传统eReader的行数/总行数方式

music:musicdrv_opts
音乐系统参数，目前支持：

MP3:
mp3_brute_mode=on/off
是否使用暴力法解码MP3，如果MP3播放有问题，试试这个，默认为否
mp3_check_crc=on/off
是否总是检查MP3的CRC检验消息，默认为否
mp3_use_me=on/off
是否使用MP3硬件解码(Media Engine）功能，否则使用libMAD进行软件解码。默认为是
mp3_buffer_size=262144
装载MP3数据的缓冲区字节大小，不得小于8192字节，默认为65536字节
AAC:
aac_buffer_size=262144
装载AAC数据的缓冲区字节大小，不得小于8192字节，默认为65536字节
WAV:
wav_buffer_size=262144
装载WAVE数据的缓冲区字节大小，不得小于8192字节，默认为65536字节
WMA:
wma_buffer_size=262144
装载WMA数据的缓冲区字节大小，不得小于8192字节，默认为65536字节
WavPack:
wv_buffer_size=262144
装载WavPack数据的缓冲区字节大小，不得小于8192字节，默认为65536字节
AT3:
at3_buffer_size=262144
装载AT3数据的缓冲区字节大小，不得小于8192字节，默认为65536字节
AA3:
aa3_buffer_size=262144
装载AA3数据的缓冲区字节大小，不得小于8192字节，默认为65536字节
M4A:
m4a_buffer_size=262144
装载M4A数据的缓冲区字节大小，不得小于8192字节，默认为65536字节
OGG:
ogg_buffer_size=262144
装载OGG数据的缓冲区字节大小，不得小于8192字节，默认为65536字节

将它们写成一行，如:
musicdrv_opts=mp3_brute_mode=on mp3_use_me=on

将使用暴力法硬件解码MP3

music:show_encoder_msg=y/n
是否显示编码器信息

image:max_cache_img
图像预读状态下最多缓冲的图像个数，默认为10，如果为0将关闭预读功能

image:use_image_queue
是否开启图像预读功能，默认为是

