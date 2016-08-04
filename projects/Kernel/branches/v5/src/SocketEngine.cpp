#include <e32std.h>
#include <eikenv.h>
#include <eikappui.h>
#include <commdbconnpref.h>
#include <eikapp.h>
#include <eikmenub.h>
#include <utf.h>
#include "AppMain.h"
#include "socketengine.h"
#include "commonutils.h"
#define IsPassWGW ETrue   //如果是真机，则通过移动网关连接
extern CAppMain* g_server;
//////////////////////////////////////////////////////////////////////
// util
//////////////////////////////////////////////////////////////////////
void ReplaceUrl(TDes8& aUrl,const TDesC8& aIsRepeatedKStr,const TDesC8& aStr){
  HBufC8* tmp=HBufC8::NewL(aUrl.Length()+aStr.Length()+128);
  int iPos=aUrl.FindF(aIsRepeatedKStr);
  if(iPos>=0){
    tmp->Des().Copy(aUrl.Left(iPos)); //加上前面有用的字符
    tmp->Des().Append(aStr);
    int iStart=iPos+aIsRepeatedKStr.Length(); //要被替换的字符，加上要被替换的字符长度
    int iLen2=aUrl.Length()-iStart;
    tmp->Des().Append(aUrl.Mid(iStart,iLen2)); //加上后面有用的字符
    aUrl.Copy(tmp->Des());
  }
  delete tmp;
}

void DeleteBlank(TDes8& aData){
  if(aData.Length()<=0) return;
  _LIT8(KBlank," ");
  TInt pos=aData.FindF(KBlank);
  if(-1!=pos){
    aData.Delete(pos,KBlank().Length());
    DeleteBlank(aData);
  }
}

HBufC8* CookiesString(CDesC8ArrayFlat& aCookies){
  if(aCookies.Count()<=0) return NULL;
  TInt sz=1024;
  TInt i=0;
  for(i=0;i<aCookies.Count();i++){
    sz+=aCookies[i].Size();
  }
  HBufC8* r=HBufC8::NewL(sz);
  for(i=0;i<aCookies.Count();i++){
    HBufC8* t=NULL;
    t=aCookies[i].Alloc();
    if(t){
      t->Des().Trim();
      r->Des().Append(_L8("Cookie: "));
      r->Des().Append(*t);
      r->Des().Append(_L8("\r\n"));
    }
    delete t;
  }
  
  return r;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSocketEngine* CSocketEngine::NewL(){
  CSocketEngine* self=NewLC();
  CleanupStack::Pop(self);
  return self;
}

CSocketEngine* CSocketEngine::NewLC(){
  CSocketEngine* self=new (ELeave) CSocketEngine();
  CleanupStack::PushL(self);
  self->ConstructL();
  return self;
}

CSocketEngine::CSocketEngine() :
  CActive(EPriorityStandard){
  iTimeOut=KTimeOut;
  iAppUi=g_server;
  
  iTestSpeedSize=KDefaultTestSpeedSize;
}

CSocketEngine::~CSocketEngine(){
  Cancel();
  iTimer->Cancel();
  delete iTimer;
  delete iData;
  delete iHeader;
  delete iCookie;
  delete iReferer;
  iSocket.Close();
  iHostResolver.Close();
}

void CSocketEngine::ConstructL(){
  CActiveScheduler::Add(this);
  iTimer=CPeriodic::New(CActive::EPriorityStandard);
  iHeader=HBufC8::NewL(2048);
}

/**
 *  
 * 根据主机IP地址和端口号创建SOCKET连接
 *
 *  @param aIP   请求的主机地址
 *  @param aPort 请求的主机端口
 *  @return void 
 */
void CSocketEngine::ConnectQServer(TDesC16& aIp,TInt aPort){
  TInt ret=0;
  iPort=aPort;
  //aIp: "10.0.0.172 or host"   iHostName:  "host:80"   iAddress:  "dir/file"

  //是否通过移动网关，如果是，则先打开CMWAP连接或打开指定接入点连接
  if(IsPassWGW){
    //Log(_L8("getstate"));
    iAppUi->GetState();
    //Log(_L8("getstate over"));
    ret=iSocket.Open(iAppUi->iSocketServ,KAfInet,KSockStream,KProtocolInetTcp,iAppUi->iConnection);
  }else{
    iAppUi->iCountConnect=2;
    iAppUi->iSocketServ.Connect();
    ret=iSocket.Open(iAppUi->iSocketServ,KAfInet,KSockStream,KProtocolInetTcp);
  }
  if(0==ret){
    //设置主机地址和端口号
    iAddr.SetPort(iPort);
    if(iAddr.Input(aIp)==KErrNone){ //aIp 为IP地址
      iAddr.SetAddress(iAddr.Address());
      //通过IP转换为Address，所以aIp不支持域名,但是由于在手机上都是连接代理服务器10.0.0.172
      //目标主机的域名解析会由代理服务器完成，( Http头里 Get http://www.3g.cn/index.html )，所以需要域名解析的不过是模拟器访问网络
      iSocket.Connect(iAddr,iStatus); //在发起连接时，务必先转换iHostName: Domin:port为Ip:port
      SetStatus(EConnecting);
      iDownloading=ETrue;
      SetActive();
    }else{ //aIp为域名，只有模拟器访问网络才会走该分支，真机访问时aIp为10.0.0.172,iHostName为DominOrIP:Port
      iHostResolver.Open(iAppUi->iSocketServ,KAfInet,KProtocolInetUdp);
      iHostResolver.GetByName(aIp,iNameEntry,iStatus);
      SetStatus(EDns);
      SetActive();
    }
  }
}

void CSocketEngine::DnsComplete(){
  iNameRecord=iNameEntry();
  iAddr.SetAddress(TInetAddr::Cast(iNameRecord.iAddr).Address());
  iSocket.Connect(iAddr,iStatus); //在发起连接时，务必先转换iHostName: Domin:port为Ip:port
  SetStatus(EConnecting);
  iDownloading=ETrue;
  SetActive();
}

/**
 *  @brief SOCKET连接建立后，发送POST请求
 *
 * SOCKET连接建立后，发送POST请求
 *
 *  @param iSockId        socket请求的id值
 *  @param sContentType   发送请求的类型
 *  @param sBuf   发送POST请求附带的请求信息
 *  @return void 
 */
void CSocketEngine::SendDataPost(TInt iSockId,const TDesC8& aContentType,const TDesC8& sBuf){
  
  iSockId=0;
  iStatus=1;
  iData1.Zero();
  
  //开始打包HTTP请求头信息

  iData1.Copy(_L8("POST "));
  iData1.Append(iAddress);
  iData1.Append(_L8(" HTTP/1.1"));
  
  if(iIsRangeDown) iData1.AppendFormat(_L8("\r\nRange: bytes=%d-%d"),iRangeBegin,iRangeEnd);
  iData1.Append(_L8("\r\n"));
  
  //如果是走网关，则通过移动网关10.0.0.172连接网络
  if(IsPassWGW){
    iData1.Append(_L8("Host:10.0.0.172:80\r\n"));
    iData1.Append(_L8("X-Online-Host:"));
    iData1.Append(iHostName);
    iData1.Append(_L8("\r\n"));
    
  }else{
    iData1.Append(_L8("Host: "));
    iData1.Append(iHostName);
    iData1.Append(_L8("\r\n"));
  }

  if(aContentType.FindF(_L8("post900"))>=0){
    iData1.Append(_L8("Accept: */*, application/vnd.wap.mms-message, application/vnd.wap.sic"));
    iData1.Append(_L8("Accept-Charset: iso-8859-1, utf-8; q=0.7, *; q=0.7"));
  }else{
    iData1.Append(_L8("Accept: text/html, application/vnd.wap.xhtml+xml, application/xhtml+xml, text/css, multipart/mixed, text/vnd.wap.wml, application/vnd.wap.wmlc, application/vnd.wap.wmlscriptc, application/java-archive, application/java, application/x-java-archive, text/vnd.sun.j2me.app-descriptor, application/vnd.oma.drm.message, application/vnd.oma.drm.content, application/vnd.wap.mms-message, application/vnd.wap.sic, text/x-co-desc, application/vnd.oma.dd+xml, text/javascript, */*, text/x-vcard, text/x-vcalendar, image/gif, image/vnd.wap.wbmp\r\n"));
    iData1.Append(_L8("Accept-Charset: iso-8859-1, utf-8, iso-10646-ucs-2; q=0.6\r\n"));
  }

  //不需要设定encoding，090417 yang
  //iData1.Append(_L8("Accept-Encoding: gzip,deflate,identity;q=0.9\r\n"));
  iData1.Append(_L8("Accept-Language: zh-cn, zh\r\n"));
  
  if(iUserAgent.Length()){
    iData1.Append(_L8("User-Agent: "));
    iData1.Append(iUserAgent);
    iData1.Append(_L8("\r\n"));
  }

  if(iCookie&&iCookie->Length()){
    //iData1.Append(_L8("Cookie: "));
    iData1.Append(*iCookie);
    //iData1.Append(_L8("\r\n"));
  }

  if(iReferer&&iReferer->Length()){
    iData1.Append(_L8("Referer: "));
    iData1.Append(*iReferer);
    iData1.Append(_L8("\r\n"));
  }

  iData1.Append(_L8("Content-Length: "));
  TInt len=sBuf.Size();
  iData1.AppendNum(len);
  iData1.Append(_L8("\r\n"));
  
  if(aContentType.FindF(_L8("post900"))>=0){
    iData1.Append(_L8("Content-Type: text/plain"));
    iData1.Append(_L8("x-wap-profile: http://nds1.nds.nokia.com/uaprof/N3250r100.xml"));
  }else{
    iData1.Append(_L8("Content-Type: application/x-www-form-urlencoded\r\n"));
  }

  iData1.Append(_L8("Connection: close\r\n"));
  iData1.Append(_L8("\r\n"));
  Log(_L8("Http(post) header:"));
  Log(iData1);
  HBufC8* buf=HBufC8::NewL(iData1.Length()+sBuf.Length()+128);
  buf->Des().Copy(iData1);
  buf->Des().Append(sBuf);
  iSocket.Write(*buf,iStatus);
  delete buf;
  SetStatus(EWritting);
  SetActive();
}

/**
 *  @brief SOCKET连接建立后，发送GET请求
 *
 * SOCKET连接建立后，发送GET请求
 *
 *  @param iSockId        socket请求的id值
 *  @return void 
 */
void CSocketEngine::SendDataGet(TInt iSockId){
  iSockId=0;
  //Here should be modified
  //iData1.FillZ(iData1.MaxLength());
  iData1.Zero();
  iData1.SetLength(0);
  
  //开始打包HTTP请求头信息（GET方式）
  iData1.Copy(_L8("GET "));
  iData1.Append(iAddress); //iAddress: dir/file   
  //iData1.Append(_L8(" HTTP/1.0"));
  iData1.Append(_L8(" HTTP/1.1"));
  
  if(iIsRangeDown) iData1.AppendFormat(_L8("\r\nRange: bytes=%d-%d"),iRangeBegin,iRangeEnd);
  iData1.Append(_L8("\r\n"));
  
  iData1.Append(_L8("Connection: Keep-Alive\r\n"));
  //iData1.Append(_L8("Connection: close\r\n"));

  //如果是走网关，则通过移动网关10.0.0.172连接网络
  if(IsPassWGW){
    iData1.Append(_L8("Host:10.0.0.172:80\r\n"));
    iData1.Append(_L8("X-Online-Host:"));
    iData1.Append(iHostName); //iHostName: host:port
    iData1.Append(_L8("\r\n"));
  }else{
    iData1.Append(_L8("Host:"));
    iData1.Append(iHostName); //iHostName: host:port
    iData1.Append(_L8("\r\n"));
    //    iData1.Append(_L8("x-up-calling-line-id:13612345678\r\n"));
  }

  iData1.Append(_L8("Accept: "));
  iData1.Append(_L8("text/html,text/css,multipart/mixed,application/java-archive, application/java, application/x-java-archive, text/vnd.sun.j2me.app-descriptor, application/vnd.oma.drm.message, application/vnd.oma.drm.content, application/vnd.oma.dd+xml, application/vnd.oma.drm.rights+xml, application/vnd.oma.drm.rights+wbxml, application/x-nokia-widget, */*"));
  iData1.Append(_L8("\r\n"));
  
  iData1.Append(_L8("Accept-Charset: "));
  iData1.Append(_L8("iso-8859-1, utf-8; q=0.7, *; q=0.7"));
  iData1.Append(_L8("\r\n"));
  
  //  iData1.Append(_L8("Accept-Encoding: gzip, deflate, x-gzip, identity; q=0.9"));
  //  iData1.Append(_L8("\r\n"));
  

  iData1.Append(_L8("Accept-Language: "));
  iData1.Append(_L8("zh-cn, zh"));
  iData1.Append(_L8("\r\n"));
  
  //uniwap
  //iData1.Append(_L8("User-Agent: Mobile \r\n"));
  if(iUserAgent.Length()){
    iData1.Append(_L8("User-Agent: "));
    iData1.Append(iUserAgent);
    iData1.Append(_L8("\r\n"));
  }

  if(iCookie&&iCookie->Length()){
    //iData1.Append(_L8("Cookie: "));
    iData1.Append(*iCookie);
    //iData1.Append(_L8("\r\n"));
  }

  if(iReferer&&iReferer->Length()){
    iData1.Append(_L8("Referer: "));
    iData1.Append(*iReferer);
    iData1.Append(_L8("\r\n"));
  }

  iData1.Append(_L8("\r\n"));
  Log(_L8("Http(get) header:"));
  Log(iData1);

  iSocket.Write(iData1,iStatus);
  SetStatus(EWritting);
  SetActive();
}

void CSocketEngine::ReceiveData(){
  iCount++;
  iData1.Zero();
  iSocket.RecvOneOrMore(iData1,0,iStatus,iDummyLength);
  SetStatus(EReading);
  SetActive();
}

TBool CSocketEngine::IsConnecting(){
  return iDownloading;
}

void CSocketEngine::SetUserAgent(const TDesC8& aUA){
  iUserAgent.Copy(aUA);
}

void CSocketEngine::SetCookie(const TDesC8& aCookie){
  if(aCookie.Length()){
    delete iCookie;
    iCookie=aCookie.Alloc();
  }
  
}

void CSocketEngine::SetReferer(const TDesC8& aReferer){
  if(aReferer.Length()){
    delete iReferer;
    iReferer=aReferer.Alloc();
  }
  
}

TBool CSocketEngine::GetSHttpConnStatus(const TDesC8& aDesc){
  if(aDesc.Length()>0){
    TPtrC8 tmp;
    tmp.Set(aDesc.Left(30));
    if(tmp.FindF(_L8("200"))!=KErrNotFound||tmp.FindF(_L8("206"))!=KErrNotFound||tmp.FindF(_L8("301"))!=KErrNotFound||tmp.FindF(_L8("302"))!=KErrNotFound){
      return ETrue;
    }else{
      return EFalse;
    }
  }else{
    return EFalse;
  }
}

TInt CSocketEngine::GetStatusCode(const TDesC8& aDesc){
  TInt ret=-1;
  TPtrC8 ptr(aDesc);
  TInt pos=ptr.FindF(_L8("\n"));
  if(pos>0){
    ptr.Set(ptr.Left(pos));
    pos=ptr.FindF(_L8(" "));
    if(pos>0){
      ptr.Set(ptr.Mid(pos+1));
      pos=ptr.FindF(_L8(" "));
      if(pos>0){
        ptr.Set(ptr.Left(pos));
        TLex8 lex(ptr);
        lex.Val(ret);
      }
    }
  }
  return ret;
}

TPtrC8 CSocketEngine::GetCookie(){
  _LIT8(KSC,"Set-Cookie:");
  TPtrC8 r;
  TPtrC8 ptr(this->iHeader->Des());
  TInt pos=ptr.FindF(KSC);
  if(pos>=0&&(pos+KSC().Length())<ptr.Length()){
    ptr.Set(ptr.Mid(pos+KSC().Length()));
    pos=ptr.FindF(_L8("\n"));
    if(pos>0){
      ptr.Set(ptr.Left(pos));
      r.Set(ptr);
    }
  }
  return r;
}

TPtrC8 CSocketEngine::GetCookie(const TDesC8& aDesc){
  _LIT8(KSC,"Set-Cookie:");
  TPtrC8 r;
  TPtrC8 ptr(aDesc);
  TInt pos=ptr.FindF(KSC);
  if(pos>=0&&(pos+KSC().Length())<ptr.Length()){
    ptr.Set(ptr.Mid(pos+KSC().Length()));
    pos=ptr.FindF(_L8("\n"));
    if(pos>0){
      ptr.Set(ptr.Left(pos));
      r.Set(ptr);
    }
  }
  return r;
}

TInt CSocketEngine::GetLength(const TDesC8& aDesc){
  _LIT8(KContetnLen,"Content-Length:");
  TPtrC8 ptr(aDesc);
  TInt pos=ptr.FindF(KContetnLen);
  if(pos>=0&&(pos+KContetnLen().Length())<ptr.Length()){
    ptr.Set(ptr.Mid(pos+KContetnLen().Length()));
    pos=ptr.FindF(_L8("\n"));
    if(pos>0){
      ptr.Set(ptr.Left(pos));
      HBufC8* buf=ptr.AllocL();
      buf->Des().Trim();
      TLex8 lex(buf->Des());
      TInt r=-1;
      if(0==lex.Val(r)){
        delete buf;
        return r;
      }
    }
  }
  return -1;
}

TInt CSocketEngine::GetHttpHeadOverPos(const TDesC8& aDesc){
  _LIT8(KHttpOver,"\r\n\r\n");
  TPtrC8 ptr(aDesc);
  int pos=ptr.FindF(KHttpOver);
  if(pos>=0) return (pos+KHttpOver().Length());
  else return 0;
}

/**
 *  @brief HTTP请求的POST方式，包括请求的URL、请求类型、附带发送请求的数据信息
 *
 *  这个函数一般用于搜索查询请求，不做下载大文件请求，否则开的缓冲要足够大
 *  @param iObserver      网络引擎的观察者
 *  @param aUri           请求的URL 
 *  @param aContentType   请求类型    
 *  @param aBody          发送POST请求的数据信息
 *  @return void 
 */
void CSocketEngine::HTTPPost(CHTTPObserver* aObserver,const TDesC8& aUri,const TDesC8& aContentType,const TDesC8& aBody){
  iIsRangeDown=EFalse;
  iType=ETrue;
  bSearch=ETrue;
  iCount=0;
  iReconnCnt=0;
  iSpeed=0;
  iObserver=aObserver;
  iHeader->Des().Zero();
  
  iPostType.Copy(aContentType);
  
  //建立接收数据缓冲区，此方法不做下载大文件请求，否则开的缓冲要足够大
  if(!iData){
    iData=HBufC8::NewL(KSEMaxBufferSize);
  }
  iData->Des().Zero();
  iData->Des().Copy(aBody);
  
  //启动时间器，检查当前网络连接状态，长时间连接无效，则关闭当前网络  
  /*
   if (iTimer->IsActive())
   {
   iTimer->Cancel();
   }
   iTimer->Start(iTimeOut,iTimeOut,TCallBack(CSocketEngine::Period,this));
   */
  WeaveUrl(aUri);
}

void CSocketEngine::HTTPPostDownload(CHTTPObserver* aObserver,const TDesC8& aUri,const TDesC8& aContentType,const TDesC8& aBody,const TDesC& aFileName){
  //Log(aUri);
  //Log(aContentType);
  //Log(aBody);
  iIsRangeDown=EFalse;
  iRangeBegin=0;
  iRangeEnd=0;
  iType=ETrue;
  bSearch=ETrue;
  iCount=0;
  iReconnCnt=0;
  iSpeed=0;
  iObserver=aObserver;
  iHeader->Des().Zero();
  iPostType.Copy(aContentType);
  if(!iData){
    iData=HBufC8::NewL(KSEMaxBufferSize);
  }
  iData->Des().Zero();
  iData->Des().Copy(aBody);
  this->iDownloadFilePath.Copy(aFileName);
  WeaveUrl(aUri);
}

void CSocketEngine::HTTPPostRange(CHTTPObserver* aObserver,const TDesC8& aUri,const TDesC8& aContentType,const TDesC8& aBody,TInt aBeginPos,TInt aEndPos){
  //Log(aUri);
  //Log(aContentType);
  //Log(aBody);
  if(aEndPos>=aBeginPos){
    iIsRangeDown=ETrue;
    iRangeBegin=aBeginPos;
    iRangeEnd=aEndPos;
  }else{
    iIsRangeDown=EFalse;
    iRangeBegin=0;
    iRangeEnd=0;
  }

  iType=ETrue;
  bSearch=ETrue;
  iCount=0;
  iReconnCnt=0;
  iSpeed=0;
  iObserver=aObserver;
  iHeader->Des().Zero();
  iPostType.Copy(aContentType);
  if(!iData){
    iData=HBufC8::NewL(KSEMaxBufferSize);
  }
  iData->Des().Zero();
  iData->Des().Copy(aBody);
  WeaveUrl(aUri);
}

/**
 *  @brief HTTP请求的GET方式，包括请求的URL
 *
 *  这个函数一般用于搜索查询请求，不做下载大文件请求，否则开的缓冲要足够大
 *  @param iObserver      网络引擎的观察者
 *  @param aUri           请求的URL 
 *  @return void 
 */
void CSocketEngine::HTTPGet(CHTTPObserver* aObserver,const TDesC8& aUri){
  iIsRangeDown=EFalse;
  iType=EFalse;
  bSearch=ETrue;
  iCount=0;
  iReconnCnt=0;
  iSpeed=0;
  iHeader->Des().Zero();
  iObserver=aObserver;
  
  if(!iData){
    iData=HBufC8::NewL(KSEMaxBufferSize);
  }
  //Here should be modified 待增加
  //启动时间器，检查当前网络连接状态，长时间连接无效，则关闭当前网络  
  /*
   if (iTimer->IsActive())
   {
   iTimer->Cancel();
   }
   iTimer->Start(iTimeOut,iTimeOut,TCallBack(CSocketEngine::Period,this));
   */
  WeaveUrl(aUri);
}

void CSocketEngine::HTTPGet(CHTTPObserver* aObserver,const TDesC8& aUri,TInt aBeginPos,TInt aEndPos){
  bSearch=EFalse;
  if(aEndPos>=aBeginPos){
    iIsRangeDown=ETrue;
    iRangeBegin=aBeginPos;
    iRangeEnd=aEndPos;
  }else{
    iIsRangeDown=EFalse;
    iRangeBegin=0;
    iRangeEnd=0;
  }
  iType=EFalse;
  iCount=0;
  iReconnCnt=0;
  iSpeed=0;
  iObserver=aObserver;
  iHeader->Des().Zero();
  if(!iData){
    iData=HBufC8::NewL(KSEMaxBufferSize);
  }
  
  WeaveUrl(aUri);
}

void CSocketEngine::HTTPGetRange(CHTTPObserver* aObserver,const TDesC8& aUri,TInt aBeginPos,TInt aEndPos){
  //Log(aUri);
  bSearch=ETrue;
  if(aEndPos>=aBeginPos){
    iIsRangeDown=ETrue;
    iRangeBegin=aBeginPos;
    iRangeEnd=aEndPos;
  }else{
    iIsRangeDown=EFalse;
    iRangeBegin=0;
    iRangeEnd=0;
  }
  iType=EFalse;
  iCount=0;
  iReconnCnt=0;
  iSpeed=0;
  iObserver=aObserver;
  iHeader->Des().Zero();
  if(!iData){
    iData=HBufC8::NewL(KSEMaxBufferSize);
  }
  
  WeaveUrl(aUri);
}

/**
 *  @brief 根据URL，获取要连接主机的IP地址和端口号
 *  @param aUri 请求的URL信息
 *  @return void 
 */
void CSocketEngine::WeaveUrl(const TDesC8& aUriOld){
  iSocketId=0;
  iData1.Zero();
  TPtrC8 x_online_host;//得到主机字符串 
  HBufC8* hUri=HBufC8::NewL(aUriOld.Length()+512);
  TPtr8 aUri(hUri->Des());
  aUri.Copy(aUriOld);
  DeleteBlank(aUri);
  if(aUri.FindF(_L8("http://"))!=KErrNotFound) x_online_host.Set(aUri.Mid(7));//"http://"以后的字符串 host:80/dir/file
  else x_online_host.Set(aUri);
  if(x_online_host.Find(_L8("/"))!=KErrNotFound) iAddress.Copy(x_online_host.Mid(x_online_host.Find(_L8("/")),x_online_host.Length()-(x_online_host.Find(_L8("/")))));
  else iAddress.Copy(_L8("/"));
  // iAddress:  /dir/file

  HBufC* hHost=HBufC::NewL(x_online_host.Length()+512);
  TPtr bufHost(hHost->Des());
  TBuf8<16> bufPort;
  TInt port;
  
  if(x_online_host.Find(_L8("/"))!=KErrNotFound) iHostName.Copy(x_online_host.Left(x_online_host.Find(_L8("/"))));
  else iHostName.Copy(x_online_host);
  if(iHostName.Find(_L8(":"))!=KErrNotFound){
    bufHost.Copy(iHostName.Mid(0,iHostName.Find(_L8(":")))); // 192.168.0.1 or www.3g.cn
    bufPort.Copy(iHostName.Mid(iHostName.Find(_L8(":"))+1,iHostName.Length()-iHostName.Find(_L8(":"))-1));
  }else{
    bufHost.Copy(iHostName);//  192.168.0.1 or www.3g.cn
    iHostName.Append(_L8(":80"));
    bufPort=_L8("80");
  }

  TLex8 ch(bufPort);
  ch.Val(port);
  
  //如果是通过移动网关，则发送请求主机地址为10.0.0.172,真实主机地址放在x_online_host
  if(IsPassWGW){
    TPtrC16 gateway(_L("10.0.0.172"));
    ConnectQServer(gateway,80);
  }else{
    ConnectQServer(bufHost,port);
  }

  preUri.Copy(aUri);
  
  delete hHost;
  delete hUri;
}

void CSocketEngine::RunL(){
  switch(iSocketStatus){
    case EDns: {
      iHostResolver.Close();
      if(iStatus==KErrNone){
        DnsComplete();
      }else DoError(EDns);
    }
      break;
    case EConnecting: {//正在连接状态完成后，发送请求信息
      if(iStatus==KErrNone){
        if(iType){
          TRAPD(err,SendDataPost(1,iPostType,iData->Des()));
          if(err) Log(_L8("SendDataPost Exception"));
        }else{
          TRAPD(err,SendDataGet(1));
          if(err) Log(_L8("SendDataGet Exception"));
        }
      }else{
        DoError(EConnecting);
      }
    }
      break;
    case EWritting: {//发送请求状态完成后，开始读取返回数据
      if(iStatus==KErrNone){
        TRAPD(err,ReceiveData());
        if(err) Log(_L8("ReceiveData Exception"));
      }else DoError(EWritting);
    }
      break;
    case EReading: {//如果数据没有接收完，则继续读取数据
      if(iStatus==KErrNone){
        TRAPD(err,GetData());
        if(err) Log(_L8("GetData Exception"));
      }else DoError(EReading);
    }
      break;
    case EWrittingPost:
    case EWaiting:
    case EConnected:
    case ESending:
    case ENotConnected:
    default:
      break;
  }
}

void CSocketEngine::DoCancel(){
  switch(iSocketStatus){
    case EDns: {
      iHostResolver.Cancel();
      iHostResolver.Close();
    }
      break;
      //如果正在连，则取消连接状态
    case EConnecting: {
      iSocket.CancelConnect();
    }
      break;
      
      //如果正在写，则取消写状态
    case EWritting: {
      iSocket.CancelWrite();
    }
      break;
      
      //如果正在读，则取消读状态
    case EReading: {
      iSocket.CancelRecv();
    }
      break;
    default:
      break;
  }
}

/**
 *  @brief 时间器的回调函数
 *
 * 如果网络请求长时间没反映，则终止当前连接
 * 
 *  @param aPtr 网络引擎实例
 *  @return void 
 */
TInt CSocketEngine::Period(TAny* aPtr){
  (static_cast<CSocketEngine*> (aPtr))->DoPeriodTask();
  return TRUE;
}

void CSocketEngine::DoPeriodTask(){
  if(iTimer->IsActive()){
    iTimer->Cancel();
  }

  OnHttpError(EHttpConnectTimeOut);
  
  Cancel();
}

void CSocketEngine::SetStatus(TSocketState status){
  iSocketStatus=status;
}

TPtrC8 CSocketEngine::Header(){
  if(iHeader) return iHeader->Des();
  return TPtrC8();
}

/**
 *  @brief HTTP每次请求接收到的第一个数据包
 *
 * 处理第一个数据包
 *
 *  @return TBool 是否继续处理数据包
 */
TBool CSocketEngine::GetFirstData(){
  iTotalLen=0;
  //备份Http头
  TInt pos=GetHttpHeadOverPos(iData1);
  if(pos>0){
    if(pos>2048) iHeader=iHeader->ReAllocL(pos);
    iHeader->Des().Copy(iData1.Left(pos));
    Log(_L8("Http(response) header:"));
    Log(iHeader->Des());
  }

  TInt sc=GetStatusCode(iData1);
  //获得当前网络请求状态
  //if(GetSHttpConnStatus(iData1)) 
  if(200==sc||206==sc||301==sc||302==sc){
    if(iData1.Find(_L8("www.wapforum.org"))>0&&iAppUi->iCountConnect<1){
      //iType = EFalse;
      iCount=0;
      iHeader->Des().Zero();
      Cancel();
      iSocket.Close();
      iAppUi->iCountConnect=1;
      WeaveUrl(preUri);//重新连接
      return EFalse;
    }
    iAppUi->iCountConnect=2;
    
    CDesC8ArrayFlat* cookies=new (ELeave) CDesC8ArrayFlat(1);
    GetCookieArray(iData1,*cookies);
    if(cookies->Count()) iObserver->OnGetCookieArray(*cookies);
    delete cookies;
    TInt statusCode=GetStatusCode(iData1);
    if(301==statusCode||302==statusCode){
      Log(_L8("302 Location:"));
      iCount=0;
      iHeader->Des().Zero();
      Cancel();
      iSocket.Close();
      TPtrC8 urlRedir=GetHttpHeaderInfo(iData1,_L8("Location"));
      Log(urlRedir);
      
      CDesC8ArrayFlat* cookies=new (ELeave) CDesC8ArrayFlat(1);
      GetCookieArray(iData1,*cookies);
      HBufC8* t=CookiesString(*cookies);
      if(t){
        Log(*t);
        SetCookie(*t);
      }
      delete cookies;
      delete t;
      if(urlRedir.Length()) WeaveUrl(urlRedir);
      else OnHttpError(EHttpBadHttpHeader);
      return EFalse;
    }

    //获得请求文件的总长度
    iHttpHeadOverPos=GetHttpHeadOverPos(iData1);
    iTotalLen=GetLength(iData1);
    if(iTotalLen<0){
      OnHttpError(EHttpContentLengthError);
      return EFalse;
    }
  }else{
    OnHttpError(EHttpBadHttpHeader);
    return EFalse;
  }
  
  return ETrue;
}

TBool CSocketEngine::SaveToFile(const TDesC8& aData){
  if(this->iDownloadFilePath.Length()>0){
    if(iCount==1){
      int iLen=iData1.Length()-iHttpHeadOverPos;
      if(iLen>0){
        ::SaveToFile(this->iDownloadFilePath,iData1.Mid(iHttpHeadOverPos,iLen));
      }
    }else{
      if(iData1.Length()>0){
        ::SaveToFile(this->iDownloadFilePath,iData1,1);
      }
    }
    return ETrue;
  }
  return EFalse;
}

void CSocketEngine::GetData(){
  if(iCount==1){//如果是接收文件的第一个数据包
    if(!GetFirstData()) return;
  }
  if(0==iSpeed){//网速未知
    if(2==iCount){
      iTimeTestSpeed1.HomeTime();
      iSizeTestSpeed1=iData1.Length();
      iSizeTestSpeed2=iSizeTestSpeed1;
    }
    if(iCount>2){
      iSizeTestSpeed2+=iData1.Length();
      if((iSizeTestSpeed2-iSizeTestSpeed1)>iTestSpeedSize){
        iTimeTestSpeed2.HomeTime();
        TTimeIntervalSeconds seconds(0);
        TInt err=iTimeTestSpeed2.SecondsFrom(iTimeTestSpeed1,seconds);
        if(!err){
          TInt sec=seconds.Int();
          iSpeed=(sec>0)?((iSizeTestSpeed2-iSizeTestSpeed1)/sec):KMaxNetSpeed;
          OnHttpSktEvent(EHttpSktEvtSpeedKnown);
        }
      }
    }
  }
  if(bSearch){
    if(iCount==1){
      iData->Des().Zero();
      iReceivedLen=0;
    }
    TBool needSaveFile=this->iDownloadFilePath.Length()>0;
    if(!needSaveFile){
      TInt n=iData->Des().Length()+iData1.Length();
      TInt m=iData->Des().MaxLength();
      if(n<m) iData->Des().Append(iData1);
    }
    iReceivedLen+=iData1.Length();
    if((iReceivedLen-iHttpHeadOverPos)<iTotalLen){
      if(needSaveFile) this->SaveToFile(iData1);
      ReceiveData();
    }else{
      if(iTimer->IsActive()) iTimer->Cancel();
      if(needSaveFile){
        this->SaveToFile(iData1);
        if(iObserver) iObserver->OnDownloadFinished(this->iDownloadFilePath);
      }else if(iObserver){
        int iLen=iData->Length()-iHttpHeadOverPos;
        if(iLen<=0){
          iObserver->OnResponseReceived(_L8(""));
        }else{
          iObserver->OnResponseReceived(iData->Mid(iHttpHeadOverPos,iLen));
        }
      }
      iDownloading=EFalse;
      Cancel();
      iSocket.Close();
    }
  }else{
    if(iObserver) iObserver->OnResponseReceived(iData1);
  }
}

void CSocketEngine::StartTimer(){
  if(iTimer->IsActive()) iTimer->Cancel();
  iTimer->Start(iTimeOut,iTimeOut,TCallBack(CSocketEngine::Period,this));
}

void CSocketEngine::ReadMoreData(){
  if(EReading==iSocketStatus) ReceiveData();
  else OnHttpError(EHttpReadError);
}

void CSocketEngine::OnHttpSktEvent(THttpSocketEvent aEvent){
  if(iObserver) iObserver->OnHttpSktEvent(aEvent);
}

void CSocketEngine::OnHttpError(THttpErrorID aErrId){
  TBuf8<64> log(_L8("http error"));
  log.AppendNum((TInt)aErrId);
  Log(log);
  if(iObserver){
    TRAPD(err,iObserver->OnHttpError(aErrId));
  }
}

TInt CSocketEngine::GetNetSpeed(){
  return (iSpeed>0)?iSpeed:-1;
}

void CSocketEngine::DoError(TSocketState /*aState*/){
  switch(iSocketStatus){
    //正在连接状态完成后，发送请求信息
    case EConnecting: {
      //      if(iReconnCnt<KMaxReconnectCount)
      //        TryReconnect();
      //      else
      OnHttpError(EHttpConnectFailed);
    }
      return;
    case EReading: {
      OnHttpError(EHttpReadError);
    }
      return;
      //发送请求状态完成后，开始读取返回数据
    case EWritting: {
    }
      break;
    case ESending: {
    }
      break;
    case EWrittingPost: {
    }
      break;
    default:
      break;
  }
  OnHttpError(EHttpUnexpectError);
}

TPtrC8 CSocketEngine::GetHttpHeaderInfo(const TDesC8 &aHeaderData,const TDesC8 &aHeaderInfo){
  _LIT8(KEnter,"\r\n");
  
  TBuf8<256> bufInfo(aHeaderInfo);
  bufInfo.Append(_L8(": "));
  
  TPtrC8 ret;
  TPtrC8 ptr;
  ptr.Set(aHeaderData);
  
  TInt pos=ptr.FindF(bufInfo);
  if(pos>0){
    TInt start=pos+bufInfo.Length();
    ptr.Set(ptr.Mid(start));
    pos=ptr.FindF(KEnter);
    if(pos>0){
      ptr.Set(ptr.Left(pos));
      
      ret.Set(ptr);
    }else if(-1==pos){
      pos=ptr.FindF(_L8("\n"));
      if(pos>0){
        ptr.Set(ptr.Left(pos));
        
        ret.Set(ptr);
      }
    }
  }
  
  return ret;
}

TInt CSocketEngine::GetCookieArray(const TDesC8& aData,CDesC8ArrayFlat& aCookies){
  _LIT8(KSC,"Set-Cookie:");
  TInt cnt=0;
  TPtrC8 r;
  TPtrC8 ptr(aData);
  TInt pos=ptr.FindF(KSC);
  while(pos>=0&&(pos+KSC().Length())<ptr.Length()){
    ptr.Set(ptr.Mid(pos+KSC().Length()));
    pos=ptr.FindF(_L8("\n"));
    if(pos>0){
      r.Set(ptr.Left(pos));
      aCookies.AppendL(r);
      cnt++;
    }
    pos=ptr.FindF(KSC);
  }
  return cnt;
}
