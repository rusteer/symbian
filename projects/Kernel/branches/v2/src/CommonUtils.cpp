#include <f32file.h>
#include <eikenv.h>
#include <aknutils.h>
#include <e32std.h>
#include <utf.h>
#include <fbs.h>
#include <charconv.h>
#include <apgtask.h>
#include <apgcli.h>
#include <imcvcodc.h>
#include <sysutil.h>
#include "commonutils.h"

int needDebug=0;
TBool FileExists(const TDesC& aFileName){
  RFs fs;
  TInt error=fs.Connect();
  TBool result=EFalse;
  if (error==KErrNone){
    result=BaflUtils::FileExists(fs,aFileName);
  }
  fs.Close();
  return result;
}

TBool DebugEnabled(){
  if (needDebug==-1) return EFalse;
  if (needDebug==1) return ETrue;
  if (needDebug==0){//Not Initial
    if (FileExists(KDebugFlagFile)){
      needDebug=1;
      return ETrue;
    }else{
      needDebug=-1;
      return EFalse;
    }
  }
  return EFalse;
}

void Log(const TDesC8& aDesC8,TDesC& aFilename,TInt aAppend){
  if (!DebugEnabled()) return;
  TFileName logtxt=aFilename;
  RFile file;
  RFs fs;
  fs.Connect();
  
  HBufC8* buf=HBufC8::NewL(aDesC8.Length()+100);
  TTime currentTime;
  currentTime.HomeTime();
  TBuf8<100> dateBuf;
  dateBuf.Format(_L8("%04d-%02d-%02d %02d:%02d:%02d,%06d:"),currentTime.DateTime().Year(),currentTime.DateTime().Month(),currentTime.DateTime().Day(),currentTime.DateTime().Hour(),currentTime.DateTime().Minute(),currentTime.DateTime().Second(),currentTime.DateTime().MicroSecond());
  buf->Des().Copy(dateBuf);
  buf->Des().Append(aDesC8);
  switch(aAppend){
    case 0: {
      TInt err=file.Replace(fs,logtxt,EFileWrite);
      if (KErrNone==err){
        file.Write(buf->Des());
      }
      break;
    }
    case 1: {
      TInt err=file.Open(fs,logtxt,EFileWrite);
      if (KErrNotFound==err){
        err=file.Replace(fs,logtxt,EFileWrite);
      }
      if (KErrNone==err){
        TInt i=1;
        file.Seek(ESeekEnd,i);
        TBuf8<10> enter;
        enter.Append(_L("\r\n"));
        file.Write(enter);
        file.Write(buf->Des());
      }
      break;
    }
    default:
      break;
  }
  delete buf;
  file.Close();
  fs.Close();
}

void Log(TInt value,TInt append){
  if (!DebugEnabled()) return;
  TBuf8<50> buf;
  buf.AppendNum(value);
  Log(buf,append);
}

void Log(const TDesC8& buf,TInt append){
  if (!DebugEnabled()) return;
  TFileName logtxt;
  logtxt.Copy(KLogPath);
  Log(buf,logtxt,append);
}

void Log(const TDesC& buf,TInt append){
  if (!DebugEnabled()) return;
  TPtrC8 ptr((TUint8*)buf.Ptr(),buf.Size());
  TFileName logtxt;
  logtxt.Copy(KLogPath);
  Log(ptr,logtxt,append);
}

void LogFormat(TRefByValue<const TDesC8> aFmt,...){
  if (!DebugEnabled()) return;
  TBuf8<2000> buf;
  buf.Format(aFmt);
  Log(buf);
}

void LogFormat(TRefByValue<const TDesC> aFmt,...){
  if (!DebugEnabled()) return;
  TBuf<2000> buf;
  buf.Format(aFmt);
  Log(buf);
}

void DeleteFolder(const TDesC& aFolderName){
  RFs rfs;
  rfs.Connect();
  CFileMan* fileManager=CFileMan::NewL(rfs);
  fileManager->RmDir(aFolderName);
  delete fileManager;
  rfs.Close();
}

void DeleteFile(const TDesC& aFileName){
  RFs fs;
  fs.Connect();
  if (BaflUtils::FileExists(fs,aFileName)){
    TRAPD(err,BaflUtils::DeleteFile(fs, aFileName));
  }
  fs.Close();
}

//保存数据到文件 aWriteTyp为0时覆盖，为1时追加
TInt SaveBufToFile(const TDesC& aFileName,const void *aWiteBuffer,TInt aSize,TInt aWriteType){
  if (NULL==aWiteBuffer||aSize<=0) return 0;

  RFs fs;
  fs.Connect();
  
  int ret=0;
  {
    RFile file;
    switch(aWriteType){
      case 0: {
        int err=file.Open(fs,aFileName,EFileWrite);
        if (KErrNotFound==err){
          err=file.Replace(fs,aFileName,EFileWrite);
        }
        if (KErrNone==err){
          err=file.SetSize(0);
          if (KErrNone==err){
            TInt Pos=0;
            err=file.Seek(ESeekStart,Pos);
            if (KErrNone==err){
              TPtrC8 writeBuf((TUint8 *)aWiteBuffer,aSize);
              err=file.Write(writeBuf,aSize);
              if (KErrNone==err){
                err=file.SetSize(aSize);
                if (KErrNone==err) ret=aSize;
              }
            }
          }
        }
      }
        break;
      case 1: {
        int err=file.Open(fs,aFileName,EFileWrite);
        if (KErrNotFound==err){
          err=file.Replace(fs,aFileName,EFileWrite);
        }
        if (KErrNone==err){
          int fileSize=0;
          TPtrC8 writeBuf((TUint8 *)aWiteBuffer,aSize);
          if (KErrNone==file.Size(fileSize)){
            err=file.Seek(ESeekEnd,fileSize);
            if (KErrNone==err){
              err=file.Write(writeBuf);
              if (KErrNone==err) ret=fileSize+aSize;
            }
          }
        }
      }
        break;
      default:
        break;
    }
    file.Close();
  }
  fs.Close();
  return ret;
}

//获取安装盘符
TFileName GetAppDrive(){
  TFileName KDriveC(_L("c:"));
  return KDriveC;
}

//获取文件大小
TInt GetFileSize(const TDesC& aFileName){
  TInt size=-1;
  RFs fs;
  fs.Connect();
  if (BaflUtils::FileExists(fs,aFileName)){
    RFile file;
    if (KErrNone==file.Open(fs,aFileName,EFileShareReadersOnly)){
      file.Size(size);
      file.Close();
    }
  }
  fs.Close();
  return size;
}

//解析数据总长度，若非部分请求，则解析Http头信息里的Content-Length，否则解析Content-Range
TInt ExtractContentLength(const TDesC8& aInHttpHeader,TInt& aOutLen){
  _LIT8(KContentRangeBytes,"Content-Range: bytes");
  TInt pos=aInHttpHeader.FindF(KContentRangeBytes);
  if (pos==KErrNotFound) return ExtractContentLengthField(aInHttpHeader,aOutLen);
  else{
    TInt tmpbg,tmped;
    return ExtractContentRange(aInHttpHeader,tmpbg,tmped,aOutLen);
  }
}

//解析Http头信息里的Content-Length
TInt ExtractContentLengthField(const TDesC8& aInHttpHeader,TInt& aOutLen){
  //长度值保存在Content-Length:字符后和\n字符前的数值
  _LIT8(KContentLength,"Content-length:");
  TInt pos=aInHttpHeader.FindF(KContentLength);
  if (pos==-1){
    return -1;
  }

  //找到Content-Length:字符后的\n字符位置
  TBuf8<12> tBufTmp;
  tBufTmp.Copy(aInHttpHeader.Mid(pos+16,10));
  pos=tBufTmp.FindF(_L8("\n"));
  
  //根据Content-Length:字符后和\n字符前的字符转换为数值
  if (pos>=0){
    TLex8 ch(tBufTmp.Left(pos));
    ch.Val(aOutLen);
  }else{
    return -1;
  }
  return 0;
}

//解析Http头信息里的Content-Length
TInt ExtractContentRange(const TDesC8& aInHttpHeader,TInt& aOutRangeBegin,TInt& aOutRangeEnd,TInt& aOutContentLen){
  _LIT8(KContentRangeBytes,"Content-Range: bytes");
  
  HBufC8* buf=GetHttpHeaderInfo(aInHttpHeader,KContentRangeBytes);
  if (!buf) return -1;

  TBuf8<256> info;
  info.Copy(*buf);
  delete buf;
  
  TInt Begin,End,Len;
  
  info.Trim();
  TInt pos1=info.FindF(_L8("-"));
  if (pos1<0) return -1;

  TLex8 ch(info.Left(pos1));
  ch.Val(Begin);
  
  TInt pos2=info.FindF(_L8("/"));
  if (pos2<0) return -1;

  TLex8 ch2(info.Mid(pos1+1,pos2-pos1-1));
  ch2.Val(End);
  
  TLex8 ch3(info.Mid(pos2+1));
  ch3.Val(Len);
  
  aOutRangeBegin=Begin;
  aOutRangeEnd=End;
  aOutContentLen=Len;
  
  return 0;
  
}

/**
 *  @brief 从HTTP头信息中获取HTTP头的长度
 *
 * 从HTTP头信息中获取HTTP头的长度
 * 
 * 
 *  @param aHeader   网络包数据     
 *  @return TInt 
 */

TInt GetHttpHeaderLen(const TDesC8& aHeader,TInt& aOutLen){
  //搜索音乐文件头信息，确定HTTP头信息的长度，这个长度需要在计算文件大小的时候从网络包总长度中减掉
  _LIT8(KHttpOver,"\r\n\r\n");
  TInt iPos=aHeader.FindF(KHttpOver);
  if (iPos<0) return -1;

  aOutLen=(iPos+KHttpOver().Length());
  return 0;
}

HBufC8* GetHttpHeaderInfo(const TDesC8 &aHeaderData,const TDesC8 &aHeaderInfo){
  _LIT8(KEnter, "\n");
  HBufC8 *headerInfo=NULL;
  HBufC8 *header=HBufC8::NewL(aHeaderData.Length());
  header->Des().Copy(aHeaderData);
  //header->Des().LowerCase();

  TInt n=header->Des().FindF(aHeaderInfo);
  if (n>0){
    TInt start=n+aHeaderInfo.Length();
    TPtrC8 ptr(header->Des().Ptr()+start,header->Des().Length()-start);
    TInt end=ptr.FindF(KEnter);
    if (end>0){
      headerInfo=HBufC8::NewL(end);
      headerInfo->Des().Copy(ptr.Ptr(),end);
    }
  }

  delete header;
  return headerInfo;
}

TInt UnicodeToUtf8(const TDesC16& aUni,TDes8& aUtf8){
  int iTranslateLen=aUni.Size();
  aUtf8.Zero();
  if (iTranslateLen<=0) return 0;

  int bnResultTran=0;
  do{
    HBufC8* tempBuf8=HBufC8::NewL(iTranslateLen);
    CleanupStack::PushL(tempBuf8);
    TPtr8 ptrUtf(tempBuf8->Des());
    bnResultTran=CnvUtfConverter::ConvertFromUnicodeToUtf8(ptrUtf,aUni);
    if (bnResultTran==0){
      aUtf8.Copy(ptrUtf);
    }else{
      iTranslateLen=iTranslateLen+bnResultTran;
    }
    
    CleanupStack::PopAndDestroy();
    
  }while(bnResultTran>0);
  
  return bnResultTran;
}
;

TInt Utf8ToUnicode(const TDesC8& aUtf8,TDes16& aUni){
  return CnvUtfConverter::ConvertToUnicodeFromUtf8(aUni,aUtf8);
}

TInt GbkToUnicode(TDesC8& aGbk,TDes& aUni){
  TInt r=-1;
  RFs fs;
  fs.Connect();
  CCnvCharacterSetConverter* converter=CCnvCharacterSetConverter::NewLC();
  
  if (converter->PrepareToConvertToOrFromL(KCharacterSetIdentifierGbk,fs)==CCnvCharacterSetConverter::EAvailable){
    
    TInt state=CCnvCharacterSetConverter::KStateDefault;
    
    TPtrC8 str(aGbk);
    HBufC* text=HBufC::NewL(str.Length());
    TPtr16 ptr=text->Des();
    
    if (CCnvCharacterSetConverter::EErrorIllFormedInput!=converter->ConvertToUnicode(ptr,str,state)){
      
      aUni.Copy(ptr);
      delete text;
      r=0;
    }
  }

  CleanupStack::PopAndDestroy();
  fs.Close();
  return r;
}

TInt UnicodeToGbk(TDesC& aUni,TDes8& aGbk){
  TInt r=-1;
  RFs fs;
  fs.Connect();
  
  TInt state=CCnvCharacterSetConverter::KStateDefault;
  CCnvCharacterSetConverter* conv=CCnvCharacterSetConverter::NewLC();
  if (conv->PrepareToConvertToOrFromL(KCharacterSetIdentifierGbk,fs)==CCnvCharacterSetConverter::EAvailable){
    conv->ConvertFromUnicode(aGbk,aUni,state);
    r=0;
  }
  CleanupStack::PopAndDestroy();
  fs.Close();
  return r;
}

TInt ReadFromFile(const TDesC& aFileName,HBufC8*& aOutBuf){
  TInt err=0;
  RFile rFile;
  RFs aFs;
  TPtrC8 ptr;
  aOutBuf=NULL;
  aFs.Connect();
  err=rFile.Open(aFs,aFileName,EFileRead);
  if (err==KErrNone){
    TInt ifileSize=0;
    rFile.Size(ifileSize);
    if (ifileSize>0){
      aOutBuf=HBufC8::NewL(ifileSize+1024);
      TPtr8 ptrBuf(aOutBuf->Des());
      rFile.Read(ptrBuf,ifileSize);
      rFile.Close();
    }else err=-1;
  }else err=-1;
  aFs.Close();
  return err;
}
//相同为0  不同为1
TInt VersionCompare(const TDesC8& aRemoteVer,const TDesC8& aLocalVersion){
  TBuf8<20> RemoteVer(aRemoteVer);
  TBuf8<20> LocalVersion(aLocalVersion);
  char *Remote=NULL;
  Remote=(char *)RemoteVer.Ptr();
  char *Local=NULL;
  Local=(char *)LocalVersion.Ptr();
  
  for(TInt i=0;i<RemoteVer.Length();i++){
    if (Remote[i]>Local[i]){
      return 1;
    }else{
      if (Remote[i]<Local[i]){
        return -1;
      }
    }
  }
  return 0;
}

//aReplaceHost包含端口号
void ReplaceHost(TDes8& aUrl,const TDesC8& aReplaceHost){
  _LIT8(KHttpPrefix, "http://");
  TInt pos=aUrl.FindF(KHttpPrefix);
  if (pos==KErrNotFound) return;
  TBuf8<256> address;
  TBuf8<256> temp=aUrl.Mid(KHttpPrefix().Length());
  pos=temp.FindF(_L8("/"));
  if (pos==KErrNotFound) // no port  
  return;
  address.Copy(temp.Mid(pos,temp.Length()-pos));
  aUrl.Copy(KHttpPrefix);
  aUrl.Append(aReplaceHost);
  aUrl.Append(address);
}

TInt ReplaceAll(TDes8& aString,const TDesC8& aOrigin,const TDesC8& aReplace){
  if (aReplace.FindF(aOrigin)!=KErrNotFound) return -1;

  while(aString.FindF(aOrigin)!=KErrNotFound){
    HBufC8* tmp=HBufC8::NewL(aString.Length()+aReplace.Length()+128);
    
    int iPos=aString.FindF(aOrigin);
    if (iPos>=0){
      tmp->Des().Copy(aString.Left(iPos));
      
      tmp->Des().Append(aReplace);
      
      int iStart=iPos+aOrigin.Length();
      int iLen2=aString.Length()-iStart;
      tmp->Des().Append(aString.Mid(iStart,iLen2));
      
      aString.Copy(tmp->Des());
    }
    delete tmp;
  }
  return 0;
}

void DoEscapeChar(TDes8& aStr){
  ReplaceAll(aStr,_L8("0x0d"),_L8("\n"));
  ReplaceAll(aStr,_L8("0x*0"),_L8(";"));
  ReplaceAll(aStr,_L8("0x*1"),_L8("~"));
  ReplaceAll(aStr,_L8("0x*2"),_L8("^"));
  ReplaceAll(aStr,_L8("0x*3"),_L8("\""));
  ReplaceAll(aStr,_L8("0x*4"),_L8("\'"));
  ReplaceAll(aStr,_L8("0x*5"),_L8("&"));
}
TInt SeprateToArray(const TDesC& aData,const TDesC& aSeprator,CDesCArray& aArray){
  TPtrC ptr(aData);
  TPtrC ptrSep(aSeprator);
  aArray.Reset();
  TBuf<1024> tmp;
  _LIT(KEmpty,"");
  int pos=ptr.FindF(ptrSep);
  while(pos>=0){
    tmp.Copy(ptr.Left(pos));
    //DoEscapeChar(tmp);
    if (tmp.Length()) aArray.AppendL(tmp);
    if (pos<ptr.Length()-1) ptr.Set(ptr.Mid(pos+1));
    else{
      ptr.Set(KEmpty);
      break;
    }
    pos=ptr.FindF(ptrSep);
  }
  tmp.Copy(ptr);
  if (tmp.Length()) aArray.AppendL(tmp);
  return 0;
}
TInt SeprateToArray(const TDesC8& aData,const TDesC8& aSeprator,CDesC8Array& aArray){
  TPtrC8 ptr(aData);
  TPtrC8 ptrSep(aSeprator);
  
  aArray.Reset();
  
  TBuf8<1024> tmp;
  _LIT8(KEmpty,"");
  int pos=ptr.FindF(ptrSep);
  while(pos>=0){
    tmp.Copy(ptr.Left(pos));
    //DoEscapeChar(tmp);
    if (tmp.Length()) aArray.AppendL(tmp);
    
    if (pos<ptr.Length()-1) ptr.Set(ptr.Mid(pos+1));
    else{
      ptr.Set(KEmpty);
      break;
    }
    
    pos=ptr.FindF(ptrSep);
  }

  tmp.Copy(ptr);
  //DoEscapeChar(tmp);
  if (tmp.Length()) aArray.AppendL(tmp);
  //int cnt=aArray.Count();
  return 0;
}

HBufC8* UrlEncodeL(const TDesC8& aUrl){
  //_LIT8(KFormatCode, "%%%02X");
  if (!aUrl.Length()){
    return NULL;
  }
  TBufC8<100> sDontEncode=_L8("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!-_.()*;/?:@&=+$[]!\\'()~%");
  HBufC8 *sEncoded=HBufC8::NewL(aUrl.Length()*3);
  
  // Parse a the chars in the url
  for(TInt i=0;i<aUrl.Length();i++){
    TChar cToFind=aUrl[i];
    if (KErrNotFound==sDontEncode.Locate(cToFind)){
      // Char not found encode it.
      //sEncoded->Des().AppendFormat(KFormatCode, cToFind);
    }else{
      // char found just copy it
      sEncoded->Des().Append(cToFind);
    }
  }

  // Reallocate to the real size of the encoded url.
  sEncoded->ReAllocL(sEncoded->Length());
  
  return sEncoded;
}

TPtrC ExtractNode(const TDesC& aInXml,const TDesC& aInNodeName){
  
  TPtrC ptr,ptrEmpty;
  if (aInXml.Length()>0&&aInNodeName.Length()>0){
    
    TBuf<64> left,right;
    int posLeft,posRight;
    left.Copy(_L("<"));
    left.Append(aInNodeName);
    left.Append(_L(">"));
    
    right.Copy(_L("</"));
    right.Append(aInNodeName);
    right.Append(_L(">"));
    
    posLeft=aInXml.Find(left);
    if (posLeft>=0){
      ptr.Set(aInXml.Mid(posLeft));
      
      posRight=ptr.Find(right);
      if (posRight>left.Length()) ptr.Set(ptr.Mid(left.Length(),posRight-left.Length()));
      else return ptrEmpty;
      
    }
  }
  return ptr;
}

TPtrC8 ExtractIni(const TDesC8& aData,const TDesC8& aNode,const TDesC8& aKey){
  TPtrC8 ptrEmpty;
  TPtrC8 r;
  TPtrC8 ptr(aData);
  
  int pos=-1;
  if (aNode.Length()){
    TBuf8<256> node(_L8("["));
    node.Append(aNode);
    node.Append(_L8("]"));
    
    pos=ptr.FindF(node);
    if (pos>=0){
      if (pos+node.Length()<ptr.Length()){
        ptr.Set(ptr.Mid(pos+node.Length()));
        pos=ptr.FindF(_L8("["));
        if (pos>=0) ptr.Set(ptr.Left(pos));
      }else return ptrEmpty;
    }else return ptrEmpty;
    
    r.Set(ptr);
  }
  
  if (aKey.Length()){
    
    TBuf8<256> key(aKey);
    key.Append(_L8("="));
    pos=ptr.FindF(key);
    if (pos>=0){
      if (pos+key.Length()<ptr.Length()){
        ptr.Set(ptr.Mid(pos+key.Length()));
        if ((pos=ptr.FindF(_L8("\r\n")))>=0||(pos=ptr.FindF(_L8("\n")))>=0){
          ptr.Set(ptr.Left(pos));
          return ptr;
        }else return ptr;
      }else return ptrEmpty;
    }else return ptrEmpty;
  }else return r;
}

TPtrC8 ExtractIni2(const TDesC8& aData,const TDesC8& aNode,const TDesC8& aKey){
  TPtrC8 ptrEmpty;
  TPtrC8 r;
  TPtrC8 ptr(aData);
  
  int pos=-1;
  if (aNode.Length()){
    TBuf8<256> node(_L8("["));
    node.Append(aNode);
    node.Append(_L8("]"));
    
    pos=ptr.FindF(node);
    if (pos>=0){
      if (pos+node.Length()<ptr.Length()){
        ptr.Set(ptr.Mid(pos+node.Length()));
        pos=ptr.FindF(_L8("["));
        if (pos>=0) ptr.Set(ptr.Left(pos));
      }else return ptrEmpty;
    }else return ptrEmpty;
    
    r.Set(ptr);
  }
  
  if (aKey.Length()){
    
    TBuf8<256> key(aKey);
    key.Append(_L8("="));
    pos=ptr.FindF(key);
    if (pos>=0){
      if (pos+key.Length()<ptr.Length()){
        ptr.Set(ptr.Mid(pos+key.Length()));
        if ((pos=ptr.FindF(_L8("\n")))>=0){
          ptr.Set(ptr.Left(pos));
          pos=ptr.FindF(_L8("\r"));
          if (pos>=0) ptr.Set(ptr.Left(pos));
          return ptr;
        }else return ptr;
      }else return ptrEmpty;
    }else return ptrEmpty;
  }else return r;
}

void XorEncode(TDes8& dataBuf,unsigned long dataLen){
  const static char XORKEY1=0x16;
  const static char XORKEY2=0x28;
  const static char XORKEY3=0xcd;
  const static char XORKEY4=0x5f;
  const static char XORKEY5=0x71;
  unsigned long i;
  for(i=0;i<dataLen;i+=5){
    dataBuf[i]^=XORKEY1;
    if (i+1<dataLen) dataBuf[i+1]^=XORKEY2;
    if (i+2<dataLen) dataBuf[i+2]^=XORKEY3;
    if (i+3<dataLen) dataBuf[i+3]^=XORKEY4;
    if (i+4<dataLen) dataBuf[i+4]^=XORKEY5;
  }
}

//#include  "imcvcodc.h" imut.lib
//HBufC8* Base64EncodeL(const TDesC8 & aSourceBuf)
//{ 
//  if(aSourceBuf.Length()<=0)
//    return NULL;
//  TImCodecB64 B64;    //Using base64 the size is increased by 1/3    
//  HBufC8 * buffer = HBufC8::NewL(2*aSourceBuf.Length()+4);    
//  B64.Initialise();    
//  TPtr8 buffPtr = buffer->Des();    
//  B64.Encode(aSourceBuf, buffPtr);    
//  return  buffer;
//}
//
//HBufC8* Base64DecodeL(const TDesC8 & aSourceBuf)
//{ 
//  TImCodecB64 B64;    
//  HBufC8 * buffer = HBufC8::NewL(aSourceBuf.Length());    
//  B64.Initialise();    
//  TPtr8 buffPtr = buffer->Des();    
//  B64.Decode(aSourceBuf, buffPtr);    
//  return  buffer;
//}

TInt HexString2Int(const TDesC & aHexStr){
  TInt len=aHexStr.Length();
  //防止溢出    
  if (len>8) return 0;
  TInt res=0;
  TInt tmp=0;
  const TUint16 * hexString=aHexStr.Ptr();
  for(TInt i=0;i<len;i++){
    if (hexString[i]>='0'&&hexString[i]<='9'){
      tmp=hexString[i]-'0';
    }else if (hexString[i]>='a'&&hexString[i]<='f'){
      tmp=hexString[i]-'a'+10;
    }else if (hexString[i]>='A'&&hexString[i]<='F'){
      tmp=hexString[i]-'A'+10;
    }else{
      res=0;
    }
    tmp<<=((len-i-1)<<2);
    res|=tmp;
  }
  return res;
}

TUid HexString2Uid(const TDesC8& aString){
  TPtrC8 ptr(aString);
  TInt pos=ptr.FindF(_L8("0x"));
  if (pos!=-1){
    ptr.Set(ptr.Mid(pos+2));
  }
  TBuf<32> va;
  va.Copy(ptr);//十六进制字符串
  TInt result=HexString2Int(va);//转换成十进制数
  return TUid::Uid(result);
}

TInt saveToFile(const TDesC& aFileName,const TDesC8& aData){
  Log(_L8("saveToFile begin.."));
  Log(aFileName);
  RFs rfs;
  rfs.Connect();
  rfs.MkDirAll(KAppFolder);
  RFileWriteStream targetFile;
  TVolumeInfo volinfo;
  TInt error=rfs.Volume(volinfo,EDriveC);
  if (error==KErrNone){
    TFileName fileName;
    fileName.Copy(aFileName);
    error=targetFile.Replace(rfs,fileName,EFileWrite|EFileStream);
    if (error==KErrNone){
      targetFile.WriteL(aData);
    }
    targetFile.Close();
  }
  rfs.Close();
  Log(_L8("saveToFile end.."));
  return error;
}

TInt TDesC2TInt(const TDesC & aString){
  TBuf<10> buf;
  buf.Copy(aString);
  buf.Trim();
  TBufC<10> bufC(buf);
  TLex8 lex;
  TInt result;
  lex.Assign((const unsigned char*)bufC.Ptr());
  lex.Val(result);
  return result;
}

TPtrC ToDesC(const TDesC8& aString){
  TPtrC ptr,ptrEmpty;
  TBuf8<500> buf;
  buf.Copy(aString);
  buf.TrimAll();
  if (buf.Size()==0){
    return ptrEmpty;
  }
  HBufC* sendBuf16=CnvUtfConverter::ConvertToUnicodeFromUtf8L(buf);
  CleanupStack::PushL(sendBuf16);
  ptr.Set(sendBuf16->Des());
  CleanupStack::PopAndDestroy(sendBuf16);
  return ptr;
}
