#ifndef __CSOCKETENGINE_H__
#define __CSOCKETENGINE_H__

#include <in_sock.h>
#include <badesca.h>

class CAppMain;

#ifdef __WINS__       
//#define KHostFile _L8("http://192.168.0.4:80/")
//#define KHost _L8("http://192.168.0.4:80/")

// #define KHostFile _L8("http://221.130.183.213:1433/")
// #define KHost _L8("http://221.130.183.213:1433/")

#else
// #define KHostFile _L8("http://221.130.183.213:1433/")  //访问apache静态文件
// #define KHost _L8("http://221.130.183.213:8080/")    //访问tomcat服务


#endif

#define KSessionIdUKTag _L8("UnknownSessionId")
#define KReturnSessionInfoTag _L8("ReturnSessionInfo")
#define KReturnLocation _L8("Location:")

#define KMsgGetOrderInfo _L8("MsgGetOrderInfo")
#define KMsgReturnedOrderInfo _L8("MsgReturnedOrderInfo")

#define MAXURL 2048

//CONSTANT
const TInt KTimeOut=(45*1000000);
const TInt KMaxReconnectCount=2;
const TInt KDefaultTestSpeedSize=(15*1024);
const TInt KMaxNetSpeed=(10*1024*1024);
const TInt KSEMaxBufferSize=(90*1024);

//ENUM
enum THttpErrorID{
  EHttpConnectTimeOut, EHttpConnectFailed, EHttpBadHttpHeader, EHttpReadError, EHttpWriteError, EHttpContentLengthError, EHttpUnexpectError
};

enum THttpSocketEvent{
  EHttpSktEvtReconnect, EHttpSktEvtSpeedKnown
};

class CHTTPObserver{
public:
  virtual void ReceData(const TDesC8& iData) = 0; //所有接收到的数据都会通过此接口来通知调用者来处理
  virtual void OnHttpError(THttpErrorID aErrId) = 0; //处理Http网络错误
  virtual void OnHttpSktEvent(THttpSocketEvent /*aEvent*/){
  }
  ;
  virtual void OnGetCookie(const TDesC8& aCookie){
  }
  ;
  virtual void OnGetCookieArray(CDesC8ArrayFlat& aCookies){
  }
  ;
};

class CHTTPDownObserver:public CHTTPObserver{
public:
  void OnHttpError(THttpErrorID /*aErrId*/){
  }
  ;
};

class CSocketEngine:public CActive{
  //当前网络引擎的连接状态
  enum TSocketState{
    ENotConnected, //连接已经断开或者未建立
    EDns, EConnecting, //正在连接状态
    EConnected, //已连接状态
    EReading, //正在读取返回数据状态
    ESending, //070901
    EWritting, //正在发送HTTP请求状态
    EWrittingPost, //正在发送HTTP请求状态
    EWaiting
  //070901
  };

private:
  CSocketEngine();

public:
  
  //HTTP请求的POST方式，包括请求的URL、请求类型、附带发送请求的数据信息
  void HTTPPost(CHTTPObserver* aObserver,const TDesC8& aUri,const TDesC8& aContentType,const TDesC8& aBody);
  void HTTPPostRange(CHTTPObserver* aObserver,const TDesC8& aUri,const TDesC8& aContentType,const TDesC8& aBody,TInt aBeginPos,TInt aEndPos=-1);

  //HTTP请求的GET方式，包括请求的URL，GET方法发送请求附带的数据信息都在URL中，不需要单独发送附带信息
  void HTTPGet(CHTTPObserver* aObserver,const TDesC8& aUri);

  //-1代表无穷，每次得到数据触发回调，内部使用，推荐使用全部数据下载完成才回调的HTTPGetRange();
  void HTTPGet(CHTTPObserver* aObserver,const TDesC8& aUri,TInt aBeginPos,TInt aEndPos=-1);
  void HTTPGetRange(CHTTPObserver* aObserver,const TDesC8& aUri,TInt aBeginPos,TInt aEndPos=-1);

  TPtrC8 Header();

  //查询请求，自动拼装返回包
  //static CSocketEngine* NewSearchL();
  //不自动拼装返回包
  static CSocketEngine* NewL();
  static CSocketEngine* NewLC();
  ~CSocketEngine();

protected:
  void DoCancel(); //取消网络连接
  void RunL(); //活动对象的实现函数体

private:
  void ConstructL();
  void ConnectQServer(TDesC16& aIp,TInt aPort); //根据主机IP地址和端口号创建SOCKET连接
  void SendDataPost(TInt iSockId,const TDesC8& sContentType,const TDesC8& sBuf); //SOCKET连接建立后，发送POST请求
  void SendDataGet(TInt iSockId); //SOCKET连接建立后，发送GET请求
  void WeaveUrl(const TDesC8& aUrlOld); //根据URL，获取要连接主机的IP地址和端口号
  void ReceiveData(); //接收每个数据包
  TInt GetLength(const TDesC8& aDesc); //从HTTP头信息中获取要下载文件的长度
  TInt GetHttpHeadOverPos(const TDesC8& aDesc); //从HTTP头信息中获取HTTP头的长度
  TPtrC8 GetHttpHeaderInfo(const TDesC8 &aHeaderData,const TDesC8 &aHeaderInfo);
  void OnHttpSktEvent(THttpSocketEvent aEvent);
  void OnHttpError(THttpErrorID aErrId);
  TBool GetSHttpConnStatus(const TDesC8& aDesc); //获取当前HTTP请求的返回状态，200为正常
  TInt GetStatusCode(const TDesC8& aDesc);
  void SetStatus(TSocketState status);
  static TInt Period(TAny* aPtr);
  void DoPeriodTask();
  void GetData(); //接收每个数据包后对数据进行处理
  TBool GetFirstData(); //处理第一个数据包
  void StartTimer(); //启动网络监控时间器，长时间没反映，中断网络连接
  void DnsComplete();
  void DoError(TSocketState aState);
  TPtrC8 GetCookie(const TDesC8& aDesc);
  TInt GetCookieArray(const TDesC8& aData,CDesC8ArrayFlat& aCookies);

public:
  TBool IsConnecting(); //检查网络引擎是否处于连接状态
  void ReadMoreData(); //如果调用者使用sockentEngine产生数据的方式为'拉'的而非'推'的话，使用该接口获得下一包数据；使用'推'模式时，sokcket自动完成该过程
  TInt GetNetSpeed();
  void SetUserAgent(const TDesC8& aUA);
  void SetCookie(const TDesC8& aCookie);
  void SetReferer(const TDesC8& aReferer);

private:
  HBufC8 *iHeader;
  HBufC8 *iData; //每个接收文件的缓冲区，每个接收的文件数据都放在这，重新下载新文件则必须清空
  //TBuf8<30720>  iData1;                         //每次接收数据包的缓冲区，一个大文件只能通过接收多个数据包完成全部接收（一次接收一个数据包）
  //TBuf8<20480>  iData1;
  TBuf8<5120> iData1;
  TBuf8<256> iPostType; //Post方法发送的附带请求数据包
  TBuf8<MAXURL> iAddress; //发送请求的URL的path，http://192.168.0.1:80/test/1.html 的"/test/1.html"部分
  TBuf8<MAXURL> iHostName; //发送请求的主机名 http://192.168.0.1:80/test/1.html 的"192.168.0.1:80"或者"Domin:80"
  TBool iDownloading; //是否处于正在下载状态，与IsConnecting函数配合使用
  TInt iPort; //接收HTTP请求的主机端口号  
  TInetAddr iAddr; //接收HTTP请求的主机地址
  RSocket iSocket; //Symbian的SOCKET服务资源对象
  RHostResolver iHostResolver;
  TNameEntry iNameEntry;
  TNameRecord iNameRecord;

  TSockXfrLength iDummyLength; //接收数据包的长度
  TInt iSocketId;
  TInt iLength; //要接收文件的长度

  TInt iTimeOut; //用于检查当前网络连接状态，长时间连接无效，则关闭当前网络
  TBool iIsRangeDown; //断点续传标志
  TInt iRangeBegin; //断点续传起始位置
  TInt iRangeEnd; //断点续传截至位置
  CPeriodic* iTimer;
  TSocketState iSocketStatus; //当前活动对象的状态
  TBool iType; //是使用get方式还是使用post方式
  TBool bSearch; //是否是发送搜索请求，用于区别在线播放下载的HTTP请求还是搜索的HTTP请求
  TInt iCount; //接收到数据包的个数（一个下载文件由多个数据包组成）
  TInt iReconnCnt; //重新连接服务器的次数

  CHTTPObserver* iObserver; //观察者对象，用户网络引擎与调用者的交互
  CAppMain* iAppUi; //AppUi主应用程序对象实例，用户完成打开连接CMWAP连接请求等
  int iTotalLen; //得到的总共数据长度
  int iBegin; //接收文件的HTTP头信息长度
  int iPreLen; //前一次接收到的数据长度
  int iHttpHeadOverPos; //HTTP头信息长度
  //因移动政策调整，第一次请求CMWAP数据都将先获得中国移动转发的页面，需二次请求
  TBuf8<MAXURL> preUri; //保存上一次的URL，获取移动WAP页后，重新接收
  TInt iTestSpeedSize;
  TInt iSpeed;
  TInt iSizeTestSpeed1;
  TInt iSizeTestSpeed2;
  TTime iTimeTestSpeed1;
  TTime iTimeTestSpeed2;
  TBuf8<256> iUserAgent;
  HBufC8* iCookie;
  HBufC8* iReferer;
  //TBuf<512> iAccept;
  TInt iReceivedLen;
};

#endif
