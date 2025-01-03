

#include <stdlib.h>
#include <stdio.h>
#include <tcl.h>
#include <string.h>
#include <EasyBeast/EasyBeastInterop.h>


char *callback_;
Tcl_Interp *interp_;

void req_to_dict(request_t* req, Tcl_Obj *objReq){
  Tcl_Obj* objHeaders = Tcl_NewDictObj();
  int headers_count;
  header_t* pt;
  
  Tcl_DictObjPut(interp_,
		 objReq,
		 Tcl_NewStringObj("path", 4),
		 Tcl_NewStringObj(req->target, strlen(req->target)));

  Tcl_DictObjPut(interp_,
		 objReq,
		 Tcl_NewStringObj("method", 6),
		 Tcl_NewStringObj(req->verb, strlen(req->verb)));

  if(req->body != NULL){
    Tcl_DictObjPut(interp_,
		   objReq,
		   Tcl_NewStringObj("body", 4),
		   Tcl_NewStringObj(req->body->body, req->body->size));
  }

  headers_count = req->headers->size;
  pt = req->headers->headers;
  while(1){
    Tcl_DictObjPut(interp_,
		   objHeaders,
		   Tcl_NewStringObj(pt->name, strlen(pt->name)),
		   Tcl_NewStringObj(pt->value, strlen(pt->value)));
    pt++;
    headers_count--;

    if(headers_count == 0) break;
  }

  Tcl_DictObjPut(interp_,
		 objReq,
		 Tcl_NewStringObj("headers", 7),
		 objHeaders);
}

int callback_eval(Tcl_Obj* objReq) {
  Tcl_Obj *objCmd = Tcl_NewObj();
  char* error;
  Tcl_ListObjAppendElement(interp_,
			   objCmd,
			   Tcl_NewStringObj(callback_, strlen(callback_)));

  Tcl_ListObjAppendElement(interp_, objCmd, objReq);

  return Tcl_EvalObjEx(interp_, objCmd, TCL_EVAL_GLOBAL);
}

response_t* server_error(const char* message){
  response_t* resp = response_new(500);
  char* error = "";
  sprintf(error, message, Tcl_ErrnoId());
  resp->body = body_new(error, NULL, strlen(error));
  resp->content_type = "text/plain";
  return resp;
}

response_t* callback_sync(request_t* req) {

  Tcl_Obj *objReq = Tcl_NewObj();
  Tcl_DictSearch dictSearch;
  Tcl_Obj *cmdResult, *objRespStatus, *objRespBody, *objRespHeaders, *objRespContentType, *dictKey, *dictVal;
  int respHeadersSize, respStatus, respBodySize = 0, done = 0;
  char* respBody;
  response_t *resp;
  header_t *pt;

  req_to_dict(req, objReq);

  if(callback_eval(objReq) != TCL_OK){
    return server_error("eval callback error: %s");
  }
  
  cmdResult = Tcl_GetObjResult(interp_);
  
  if (Tcl_DictObjGet(interp_, cmdResult, Tcl_NewStringObj("status", 6), &objRespStatus) != TCL_OK){
    return server_error("get dict key 'status' error: %s");
  }

  if (Tcl_DictObjGet(interp_, cmdResult, Tcl_NewStringObj("body", 4), &objRespBody) != TCL_OK){
    return server_error("get dict key 'body' error: %s");
  }

  /*
  if (Tcl_DictObjGet(interp_, cmdResult, Tcl_NewStringObj("content_type", 12), &objRespContentType) != TCL_OK){
    return server_error("get dict key 'content_type' error: %s");
  }
  */

  if(Tcl_DictObjGet(interp_, cmdResult, Tcl_NewStringObj("headers", 7), &objRespHeaders) != TCL_OK){
    return server_error("get dict key 'headers' error: %s");
  }

  if(Tcl_DictObjSize(interp_, objRespHeaders, &respHeadersSize) != TCL_OK){
    return server_error("get headers dict size error: %s");
  }

  if(Tcl_GetIntFromObj(interp_, objRespStatus, &respStatus) != TCL_OK){
    return server_error("get status as int error: %s");
  }
  
  respBody = Tcl_GetStringFromObj(objRespBody, &respBodySize);

  resp = response_new(respStatus);

  if(respBodySize > 0){
    resp->body = body_new(respBody, NULL, respBodySize);
  }

  //resp->content_type = Tcl_GetString(objRespContentType);

  if(respHeadersSize > 0){
    resp->headers = headers_new(respHeadersSize);
  
    if(Tcl_DictObjFirst(interp_, objRespHeaders, &dictSearch, &dictKey, &dictVal, &done) != TCL_OK) {
      return server_error("dict first error: %s");
    }

    pt = resp->headers->headers;

    while(done == 0){
      pt->name = Tcl_GetString(dictKey);
      pt->value = Tcl_GetString(dictVal);
      pt++;
      Tcl_DictObjNext(&dictSearch, &dictKey, &dictVal, &done);
    }
  }

  return resp;
}


int
easy_beast_serve(ClientData data, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[]);

int
Easy_beast_Init(Tcl_Interp *interp)
{
    Tcl_Namespace *nsPtr;

    if(Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL){
        return TCL_ERROR;
    }

    nsPtr = Tcl_CreateNamespace(interp, "EasyBeast", NULL, NULL);

    if(nsPtr == NULL) {
        return TCL_ERROR;
    }

    Tcl_CreateObjCommand(interp, "EasyBeast::serve", easy_beast_serve, NULL, NULL);

    //Tcl_Export(interp, nsPtr, "parse", 0);

    if(Tcl_PkgProvide(interp, "EasyBeast", "0.1") == TCL_ERROR) {
        return TCL_ERROR;
    }

    return TCL_OK;
}


/**
 * @brief http_parser_parse_request
 * Extract channel content and try parse content by pico http parser. Return a dict with keys:
 *  - method: string
 *  - path: string
 *  - version: string
 *  - headers: dict
 *  - body: string
 * @param data
 * @param interp
 * @param objc
 * @param objv
 * @return Success or failure
 */
int
easy_beast_serve(ClientData data, Tcl_Interp *interp, int objc, Tcl_Obj * const objv[])
{
  int resultCode = TCL_OK, port, thread_count = 1;
  char *hostname, *opt, *callback;
    
  Tcl_Channel channel;

  if(objc < 2 || objc > 5) {
    Tcl_WrongNumArgs(interp, 1, objv, "Usage: serve ?hostname? ?port? ?calback? -threads ?threads_count?");
    return TCL_ERROR;
  }

  hostname = Tcl_GetString(objv[1]);
  if(Tcl_GetIntFromObj(interp, objv[2], &port) == TCL_ERROR) {
    return TCL_ERROR;
  }

  callback = Tcl_GetString(objv[3]);

  if(objc > 4) {
    opt = Tcl_GetString(objv[4]);
    if(strcmp(opt, "-threads")){
      if(Tcl_GetIntFromObj(interp, objv[4], &thread_count) == TCL_ERROR) {
	return TCL_ERROR;
      }
    } else {
      Tcl_WrongNumArgs(interp, 1, objv, "Usage: serve ?hostname? ?port? ?callback? -threads ?threads_count?");
      return TCL_ERROR;
    }
  }

  callback_ = callback;
  interp_ = interp;

  run_sync(hostname, port, thread_count, NULL, &callback_sync);

  return TCL_OK;
}
