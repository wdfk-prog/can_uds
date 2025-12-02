from building import *
import rtconfig

cwd     = GetCurrentDir()

src     = Glob('*.c')
path    = [cwd]
path    += [cwd + '/service']

if GetDepend('UDS_USING_EXAMPLE'):
    src += Glob('examples/*.c')

if GetDepend('UDS_ENABLE_SESSION_SVC'):
    src += Glob('service/service_0x10_session.c')

if GetDepend('UDS_ENABLE_SECURITY_SVC'):
    src += Glob('service/service_0x27_security.c')

if GetDepend('UDS_ENABLE_CONSOLE_SVC'):
    src += Glob('service/service_0x31_console.c')

if GetDepend('UDS_ENABLE_FILE_SVC'):
    src += Glob('service/service_0x36_0x37_0x38_file.c')

if GetDepend('UDS_ENABLE_PARAM_SVC'):
    src += Glob('service/service_0x22_0x2E_param.c')

if GetDepend('UDS_ENABLE_0X2F_IO_SVC'):
    src += Glob('service/service_0x2F_io.c')

if GetDepend('UDS_ENABLE_0X11_RESET_SVC'):
    src += Glob('service/service_0x11_reset.c')

if GetDepend('UDS_ENABLE_0X28_COMM_CTRL_SVC'):
    src += Glob('service/service_0x28_comm.c')

group = DefineGroup('can_uds', src, depend = ['PKG_USING_ISO14229'], CPPPATH = path)

Return('group')
