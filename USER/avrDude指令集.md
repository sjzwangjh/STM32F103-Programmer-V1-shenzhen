# 1. 自动查询器件型号
.\avrdude.exe -C "E:\wangjunhua\Project\AvrProgrammer\avrdude\out\build\x64-Release\src\avrdude.conf" -c stk500v2 -P avrdoper -p m8
# 2. 编程文件 E:\wangjunhua\Project\C32U4\Makey_Readed.hex
.\avrdude.exe -C "E:\wangjunhua\Project\AvrProgrammer\avrdude\out\build\x64-Release\src\avrdude.conf" -c stk500v2 -p m32u4 -P avrdoper -U flash:w:"E:\wangjunhua\Project\C32U4\Makey_Readed.hex":a 
# 3. 读取熔丝位
.\avrdude.exe -C "E:\wangjunhua\Project\AvrProgrammer\avrdude\out\build\x64-Release\src\avrdude.conf" -c stk500v2 -p m32u4 -P avrdoper -U hfuse:r:-:h -U lfuse:r:-:h -U efuse:r:-:h 
# 4. 编程熔丝位 E1 D9 (低-高)
.\avrdude.exe -C "E:\wangjunhua\Project\AvrProgrammer\avrdude\out\build\x64-Release\src\avrdude.conf" -c stk500v2 -p m32u4 -P avrdoper -U lfuse:w:0xE1:m -U hfuse:w:0xD9:m 
# 5. 编程扩展熔丝位 FF
.\avrdude.exe -C "E:\wangjunhua\Project\AvrProgrammer\avrdude\out\build\x64-Release\src\avrdude.conf" -c stk500v2 -p m32u4 -P avrdoper -U lock:w:0xFF:m 
# 6. 读取扩展熔丝位
.\avrdude.exe -C "E:\wangjunhua\Project\AvrProgrammer\avrdude\out\build\x64-Release\src\avrdude.conf" -c stk500v2 -p m32u4 -P avrdoper -U lock:r:-:h 
# 7. 读取Flash保存到 “E:\wangjunhua\Project\C32U4\Makey_Readed_Avrdude.hex”文件内
.\avrdude.exe -C "E:\wangjunhua\Project\AvrProgrammer\avrdude\out\build\x64-Release\src\avrdude.conf" -c stk500v2 -p m32u4 -P avrdoper -U flash:r:"E:\wangjunhua\Project\C32U4\Makey_Readed_Avrdude.hex":i 
