# -*- coding: UTF-8 -*- 
import freeswitch
import datetime
import json

#执行python脚本需要按照 mod_python模块


# 网关名，在sipprofile中配置的
default_gw = "gatewag_01/"
slave_gw = "gatewag_02/"

api = freeswitch.API()

# 对号码进行预处理
def preJobs(tenantId, number, enflag):
    payload = {}
    payload["tenantId"] = tenantId
    payload["number"] = number
    url = "http://127.0.0.1:8080/action/any content-type 'application/json' post '" +  json.dumps(payload)+"'"
    freeswitch.consoleLog("info","url:" + url)
    response = api.execute("curl", url)  #进行curl操作，需要依赖 mod_curl
    freeswitch.consoleLog("info", "response:"+response)
    user_dict = eval(response)
    tempPhone = user_dict['data']
    freeswitch.consoleLog("info", "tempPhone:"+tempPhone)
    return tempPhone

#挂断电话后触发该函数
def hangup_hook(session, what, args):
    freeswitch.consoleLog("info", "==============hangup hook.args:{}".format(args))
    return "Result"

def getRecordPath(tenantId,caller,callee):
    date = datetime.datetime.now().strftime('%Y%m%d')
    dateDetail = datetime.datetime.now().strftime('%Y%m%d%H%M%S')
    baseDir = api.executeString("eval $${base_dir}")
    recordPath = "%s/recordings/%s/%s/%s/%s_%s.wav" % (baseDir,tenantId,caller,date,dateDetail,callee)
    return recordPath

def handler(session, args):
    # 获取webrtc传入的一些参数
    caller = session.getVariable("caller_id_number")
    destination_number = session.getVariable("destination_number")
    callee = destination_number[0:]
    tenantId = session.getVariable("sip_h_X-tenantId")
    uniqueId = session.getVariable("sip_h_X-uniqueId")
    callback = session.getVariable("sip_h_X-callback")
    # 设置通道变量
    session.setVariable("tenantId", tenantId)
    session.setVariable("uniqueId", uniqueId)
    
    session.setVariable("callback", callback)
    session.setVariable("calleenum", callee)
    session.setVariable("callernum", caller)
    # 添加一些自定义变量
    session.setVariable("callType", "2")
    session.setVariable("role", "1")

    freeswitch.consoleLog("info","testCall---------------------"+caller)
    

    # 对号码进行预处理
    callee = preJobs(tenantId, destination_number, enflag)

    

    freeswitch.consoleLog("info","===========callee: "+callee)
    session.setVariable("calleenum", callee)

    #bridge过程中，当被叫挂断后，主机session已经存在的，此时不会触发主叫的HangupHook，如果要触发则必须主动让主机挂机
    #方案一：当被叫挂断后，主叫也字段挂断。在主叫中设置 session.setVariable("hangup_after_bridge","true")
    #方案二：在脚本最后直接调用session挂掉主叫： session.hangup()
    session.setVariable("hangup_after_bridge","true")
    #设置挂断回调钩子，支持传入自定义参数
    session.setHangupHook(hangup_hook,"12312312")


    # 使用 uuid_record 进行录音
    uuid = session.getVariable("uuid")
    cmd = "uuid_record " + uuid +" start "+ recordPath
    freeswitch.consoleLog("info","===========cmd: "+cmd)

    if session.ready():
        recordPath = getRecordPath(tenantId,caller,callee)
        freeswitch.consoleLog("info","===========recordPath: "+recordPath)
        channelVariable = "ignore_sdp_ice=true,callernum=%s,calleenum=%s,tenantId=%s,uniqueId=%s,callback=%s,appId=%s,transparent_data=%s,callType=2,role=2" % (caller,callee,tenantId,uniqueId,callback,appId,transparent_data)
        freeswitch.consoleLog("info","===========channelVariable: "+channelVariable)
        session.setVariable("continue_on_fail", "GATEWAY_DOWN,INVALID_GATEWAY")

        dialString = "{%s,execute_on_answer1='set recordPath=%s',execute_on_answer2='record_session %s'}sofia/gateway/%s%s" % (channelVariable, recordPath, recordPath, default_gw, callee)
        freeswitch.consoleLog("info","dialString:"+dialString)
        session.execute("bridge", dialString)
        cause = session.getVariable("DIALSTATUS")
        freeswitch.consoleLog("info","===========cause: "+cause)

    # 主叫主动挂断，当设置hangup_after_bridge则不需要再显式挂断
    session.hangup()