# -*- coding: UTF-8 -*- 
import freeswitch
import datetime
import json

#ִ��python�ű���Ҫ���� mod_pythonģ��


# ����������sipprofile�����õ�
default_gw = "gatewag_01/"
slave_gw = "gatewag_02/"

api = freeswitch.API()

# �Ժ������Ԥ����
def preJobs(tenantId, number, enflag):
    payload = {}
    payload["tenantId"] = tenantId
    payload["number"] = number
    url = "http://127.0.0.1:8080/action/any content-type 'application/json' post '" +  json.dumps(payload)+"'"
    freeswitch.consoleLog("info","url:" + url)
    response = api.execute("curl", url)  #����curl��������Ҫ���� mod_curl
    freeswitch.consoleLog("info", "response:"+response)
    user_dict = eval(response)
    tempPhone = user_dict['data']
    freeswitch.consoleLog("info", "tempPhone:"+tempPhone)
    return tempPhone


def handler(session, args):
    # ��ȡwebrtc�����һЩ����
    caller = session.getVariable("caller_id_number")
    destination_number = session.getVariable("destination_number")
    callee = destination_number[0:]
    tenantId = session.getVariable("sip_h_X-tenantId")
    uniqueId = session.getVariable("sip_h_X-uniqueId")
    callback = session.getVariable("sip_h_X-callback")
    # ����ͨ������
    session.setVariable("tenantId", tenantId)
    session.setVariable("uniqueId", uniqueId)
    
    session.setVariable("callback", callback)
    session.setVariable("calleenum", callee)
    session.setVariable("callernum", caller)
    # ���һЩ�Զ������
    session.setVariable("callType", "2")
    session.setVariable("role", "1")

    freeswitch.consoleLog("info","testCall---------------------"+caller)
    

    # �Ժ������Ԥ����
    callee = preJobs(tenantId, destination_number, enflag)

    

    freeswitch.consoleLog("info","===========callee: "+callee)
    session.setVariable("calleenum", callee)


    if session.ready():
        date = datetime.datetime.now().strftime('%Y%m%d')
        dateDetail = datetime.datetime.now().strftime('%Y%m%d%H%M%S')
        baseDir = api.executeString("eval $${base_dir}")  #��ȡfreeswitch�Ĺ���·��
        recordPath = "%s/recordings/%s/%s/%s/%s_%s.wav" % (baseDir,tenantId,caller,date,dateDetail,callee)
        freeswitch.consoleLog("info","===========recordPath: "+recordPath)
        channelVariable = "ignore_sdp_ice=true,callernum=%s,calleenum=%s,tenantId=%s,uniqueId=%s,callback=%s,appId=%s,transparent_data=%s,callType=2,role=2" % (caller,callee,tenantId,uniqueId,callback,appId,transparent_data)
        freeswitch.consoleLog("info","===========channelVariable: "+channelVariable)
        session.setVariable("continue_on_fail", "GATEWAY_DOWN,INVALID_GATEWAY")

        dialString = "{%s,execute_on_answer1='set recordPath=%s',execute_on_answer2='record_session %s'}sofia/gateway/%s%s" % (channelVariable, recordPath, recordPath, default_gw, callee)
        freeswitch.consoleLog("info","dialString:"+dialString)
        session.execute("bridge", dialString)
        cause = session.getVariable("DIALSTATUS")
        freeswitch.consoleLog("info","===========cause: "+cause)
