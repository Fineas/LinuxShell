from pwn import *

context.log_level = 'DEBUG'

p = remote('127.0.0.1',31337)

# p.interactive()

a = '''
\x1b\x5b\x33\x36\x6d\x1b\x5b\x34\x30\x6d\x20\xf0\x9f\x90\x9a\x20\x1b\x5b\x30\x6d\x1b\x5b\x33\x37\x6d\x1b\x5b\x34\x36\x6d\x20\x75\x73\x65\x72\x40\x6c\x6f\x63\x61\x6c\x5f\x73\x65\x73\x73\x69\x6f\x6e\x20\x1b\x5b\x30\x6d\x1b\x5b\x33\x36\x6d\x1b\x5b\x34\x30\x6d\x20\x3e\x20\x1b\x5b\x30\x6d
'''

print len(a)