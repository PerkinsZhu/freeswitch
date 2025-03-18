# 导入FreeSWITCH的Python模块以实现与FreeSWITCH服务器的交互
import freeswitch

"""
FreeSWITCH的mod_python使用示例。此模块使用mod_python默认查找的名称，
但大多数这些名称可以通过在从FreeSWITCH调用模块时使用<modname>::<function>来覆盖。
"""
def handler(session, args):
        """
        'handler'是应用程序的默认函数名。它可以被<modname>::<function>覆盖。
        `session`是会话对象，用于控制通话过程。
        `args`是一个字符串，包含了模块名之后的所有传入参数。
        
        功能：接听电话呼叫，记录日志，设置挂机钩子，设置输入回调，并播放音乐。
        """
        freeswitch.consoleLog('info', 'Answering call from Python.\n')  # 记录接听电话的日志信息
        freeswitch.consoleLog('info', 'Arguments: %s\n' % args)         # 打印传入的参数
        
        session.answer()                                                  # 接听电话
        session.setHangupHook(hangup_hook)                                # 设置挂机时的回调函数
        session.setInputCallback(input_callback)                           # 设置输入（如DTMF）的回调函数
        session.execute("playback", session.getVariable("hold_music"))     # 播放背景音乐，音乐来源为变量hold_music的值



def hangup_hook(session, what, args=''):
    """
    处理挂机或转移事件的钩子函数，需通过session.setHangupHook事先设定。
    `session`: 会话对象。
    `what`: 字符串，表示触发事件的类型，如"hangup"（挂断）或"transfer"（转移）。
    `args`: 可选参数，若在设置回调时提供了额外参数，则此处会有值。
    """
    freeswitch.consoleLog("info", "hangup hook for '%s'\n" % what)  # 记录挂机或转移的钩子被触发的日志信息


def input_callback(session, what, obj, args=''):
    """
    处理输入事件（如DTMF按键）的回调函数，需通过session.setInputCallback事先设定。
    `session`: 会话对象。
    `what`: 字符串，表示事件类型，如"dtmf"（双音多频按键）或"event"（其他事件）。
    `obj`: 对象，根据what的不同，可能是DTMF对象或事件对象。
    `args`: 可选参数，同hangup_hook。
    
    功能：根据输入类型记录日志，并根据情况暂停输入处理。
    """
    if what == "dtmf":                          # 判断是否为DTMF输入
        freeswitch.consoleLog("info", what + " " + obj.digit + "\n")  # 记录按下的DTMF键
    else:
        freeswitch.consoleLog("info", what + " " + obj.serialize() + "\n")  # 记录其他类型的事件
    return "pause"  # 返回"pause"以暂停输入处理，等待进一步指令


def fsapi(session, stream, env, args):
    """
    处理来自fs_cli、拨号计划HTTP请求等的API调用。
    `session`: 当从拨号计划调用时为会话对象，否则为"na"。
    `stream`: 输出流对象，用于向API调用方返回数据。
    `env`: 环境事件对象，包含调用时的相关环境信息。
    `args`: 调用该模块时传入的所有参数组成的字符串。
    
    功能：根据是否有参数，记录不同日志，并返回环境事件的序列化数据。
    """
    if args:
        stream.write("fsapi called with no arguments.\n")  # 若有参数，提示无参数调用（这里可能是注释错误，应为有参数）
    else:
        stream.write("fsapi called with these arguments: %s\n" % args)  # 记录传入的参数
    stream.write(env.serialize())  # 将环境事件对象序列化后返回给调用者


def runtime(args):
    """
    在独立线程中运行指定函数，通常由fs_cli的`pyrun`命令触发。
    `args`: 从命令行传递给此函数的参数字符串。
    
    功能：简单打印传入的参数。
    """
    print(args + "\n")  # 打印参数并换行


def xml_fetch(params):
    """
    绑定到FreeSWITCH的XML查找功能，用于动态生成或修改XML配置。
    `params`: 包含查找请求详情的事件对象。
    
    功能：返回一个示例XML配置，定义了一个简单的拨号计划上下文，用于接听电话并播放音乐。
    """
    xml = '''<?xml version="1.0" encoding="UTF-8" standalone="no"?>
    <document type="freeswitch/xml">
        <section name="dialplan" description="RE Dial Plan For FreeSWITCH">
            <context name="default">
                <extension name="generated">
                    <condition>
                        <action application="answer"/>
                        <action application="playback" data="${hold_music}"/>
                    </condition>
                </extension>
            </context>
        </section>
    </document>'''
    return xml  # 返回生成的XML配置
