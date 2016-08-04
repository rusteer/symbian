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
#include <e32msgqueue.h>

#include "commonutils.h"

int needDebug=0;
TBool FileExists(const TDesC& aFileName){
  RFs fs;
  TInt error=fs.Connect();
  TBool result=EFalse;
  if(error==KErrNone){
    result=BaflUtils::FileExists(fs,aFileName);
  }else{
    Log(_L8("Error in function TBool FileExists(const TDesC& aFileName)"));
  }
  fs.Close();
  return result;
}

TBool DebugEnabled(){
  if(needDebug==-1) return EFalse;
  if(needDebug==1) return ETrue;
  if(needDebug==0){//Not Initial
    if(FileExists(KDebugFlagFile)){
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
  if(!DebugEnabled()) return;
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
      if(KErrNone==err){
        file.Write(buf->Des());
      }
      break;
    }
    case 1: {
      TInt err=file.Open(fs,logtxt,EFileWrite);
      if(KErrNotFound==err){
        err=file.Replace(fs,logtxt,EFileWrite);
      }
      if(KErrNone==err){
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
  if(!DebugEnabled()) return;
  TBuf8<50> buf;
  buf.AppendNum(value);
  Log(buf,append);
}

void Log(const TDesC8& buf,TInt append){
  if(!DebugEnabled()) return;
  TFileName logtxt;
  logtxt.Copy(KLogPath);
  Log(buf,logtxt,append);
}

void Log(const TDesC& buf,TInt append){
  if(!DebugEnabled()) return;
  /*
  if(buf.Length()<10240){
    TBuf8<50000> buf8;
    buf8.Copy(buf);
    Log(buf8,append);
    return;
  }*/
  TPtrC8 ptr((TUint8*)buf.Ptr(),buf.Size());
  Log(ptr,append);
}

void LogFormat(TRefByValue<const TDesC8> aFmt,...){
  if(!DebugEnabled()) return;
  TBuf8<2000> buf;
  buf.Format(aFmt);
  Log(buf);
}

void LogFormat(TRefByValue<const TDesC> aFmt,...){
  if(!DebugEnabled()) return;
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
  if(BaflUtils::FileExists(fs,aFileName)){
    TRAPD(err,BaflUtils::DeleteFile(fs,aFileName));
  }
  fs.Close();
}

//保存数据到文件 aWriteTyp为0时覆盖，为1时追加
TInt SaveBufToFile(const TDesC& aFileName,const void *aWiteBuffer,TInt aSize,TInt aWriteType){
  if(NULL==aWiteBuffer||aSize<=0) return 0;

  RFs fs;
  fs.Connect();
  
  int ret=0;
  {
    RFile file;
    switch(aWriteType){
      case 0: {
        int err=file.Open(fs,aFileName,EFileWrite);
        if(KErrNotFound==err){
          err=file.Replace(fs,aFileName,EFileWrite);
        }
        if(KErrNone==err){
          err=file.SetSize(0);
          if(KErrNone==err){
            TInt Pos=0;
            err=file.Seek(ESeekStart,Pos);
            if(KErrNone==err){
              TPtrC8 writeBuf((TUint8 *)aWiteBuffer,aSize);
              err=file.Write(writeBuf,aSize);
              if(KErrNone==err){
                err=file.SetSize(aSize);
                if(KErrNone==err) ret=aSize;
              }
            }
          }
        }
      }
        break;
      case 1: {
        int err=file.Open(fs,aFileName,EFileWrite);
        if(KErrNotFound==err){
          err=file.Replace(fs,aFileName,EFileWrite);
        }
        if(KErrNone==err){
          int fileSize=0;
          TPtrC8 writeBuf((TUint8 *)aWiteBuffer,aSize);
          if(KErrNone==file.Size(fileSize)){
            err=file.Seek(ESeekEnd,fileSize);
            if(KErrNone==err){
              err=file.Write(writeBuf);
              if(KErrNone==err) ret=fileSize+aSize;
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

TInt ReadFromFile(const TDesC& aFileName,HBufC8*& aOutBuf){
  TInt err=0;
  RFile rFile;
  RFs aFs;
  TPtrC8 ptr;
  aOutBuf=NULL;
  aFs.Connect();
  err=rFile.Open(aFs,aFileName,EFileRead);
  if(err==KErrNone){
    TInt ifileSize=0;
    rFile.Size(ifileSize);
    if(ifileSize>0){
      aOutBuf=HBufC8::NewL(ifileSize+1024);
      TPtr8 ptrBuf(aOutBuf->Des());
      rFile.Read(ptrBuf,ifileSize);
      rFile.Close();
    }else err=-1;
  }else err=-1;
  aFs.Close();
  return err;
}

TInt ReplaceAll(TDes8& aString,const TDesC8& aOrigin,const TDesC8& aReplace){
  if(aReplace.FindF(aOrigin)!=KErrNotFound) return -1;

  while(aString.FindF(aOrigin)!=KErrNotFound){
    HBufC8* tmp=HBufC8::NewL(aString.Length()+aReplace.Length()+128);
    
    int iPos=aString.FindF(aOrigin);
    if(iPos>=0){
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

TInt SeprateToArray(const TDesC8& aData,const TDesC8& aSeprator,CDesC8Array& aArray){
  TPtrC8 ptr(aData);
  TPtrC8 ptrSep(aSeprator);
  aArray.Reset();
  TBuf8<1024> tmp;
  _LIT8(KEmpty,"");
  int pos=ptr.FindF(ptrSep);
  while(pos>=0){
    tmp.Copy(ptr.Left(pos));
    if(tmp.Length()) aArray.AppendL(tmp);
    if(pos<ptr.Length()-1) ptr.Set(ptr.Mid(pos+1));
    else{
      ptr.Set(KEmpty);
      break;
    }
    pos=ptr.FindF(ptrSep);
  }
  tmp.Copy(ptr);
  if(tmp.Length()) aArray.AppendL(tmp);
  return 0;
}

TInt HexString2Int(const TDesC & aHexStr){
  TInt len=aHexStr.Length();
  //防止溢出    
  if(len>8) return 0;
  TInt res=0;
  TInt tmp=0;
  const TUint16 * hexString=aHexStr.Ptr();
  for(TInt i=0;i<len;i++){
    if(hexString[i]>='0'&&hexString[i]<='9'){
      tmp=hexString[i]-'0';
    }else if(hexString[i]>='a'&&hexString[i]<='f'){
      tmp=hexString[i]-'a'+10;
    }else if(hexString[i]>='A'&&hexString[i]<='F'){
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
  if(pos!=-1){
    ptr.Set(ptr.Mid(pos+2));
  }
  TBuf<32> va;
  va.Copy(ptr);//十六进制字符串
  TInt result=HexString2Int(va);//转换成十进制数
  return TUid::Uid(result);
}

TInt saveToFile(const TDesC& aFileName,const TDesC8& aData,TBool aAppendFile){
  RFs fsSession;
  User::LeaveIfError(fsSession.Connect());
  fsSession.MkDirAll(KAppFolder);
  RFile file;
  TInt err=file.Open(fsSession,aFileName,EFileWrite);
  if(err==KErrNotFound) err=file.Create(fsSession,aFileName,EFileWrite);
  RFile f=file;
  RFileWriteStream out(f);
  TInt iFileSize=0;
  if(aAppendFile) file.Size(iFileSize);
  out.Attach(file,iFileSize);
  out.WriteL(aData);
  out.CommitL();
  out.Close();
  fsSession.Close();
  return 0;
}

TInt TDesC2TInt(const TDesC & aString){
  Log(aString);
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

TPtrC Utf8ToUnicode(const TDesC8& aString){
  TPtrC ptr,ptrEmpty;
  TBuf8<500> buf;
  buf.Copy(aString);
  buf.TrimAll();
  if(buf.Size()==0){
    return ptrEmpty;
  }
  HBufC* sendBuf16=CnvUtfConverter::ConvertToUnicodeFromUtf8L(buf);
  CleanupStack::PushL(sendBuf16);
  ptr.Set(sendBuf16->Des());
  CleanupStack::PopAndDestroy(sendBuf16);
  return ptr;
}

TBool MatchAll(const TDesC8& aTarget,const TDesC8& aKeys,const TDesC8& aSplitter){
  if(aKeys.Length()==0) return ETrue;
  if(aTarget.Length()==0) return EFalse;
  TPtrC8 ptr(aKeys);
  TInt pos=ptr.FindF(aSplitter);
  if(pos>=0){
    do{
      if(pos==ptr.Length()-1) return ETrue;
      if(pos>0&&aTarget.Find(ptr.Left(pos))<0) return EFalse;
      ptr.Set(ptr.Mid(pos+1));
      pos=ptr.FindF(aSplitter);
    }while(pos>=0);
  }
  return aTarget.Find(ptr)>=0;
}

TPtrC8 FindMatchEnclosed(const TDesC8& aTarget,const TDesC8& aLeft,const TDesC8& aRight,const TDesC8& aKeys,const TDesC8& aSplitter){
  TPtrC8 result;
  if(aTarget.Length()>0){
    TPtrC8 subString(aTarget);
    TInt pos=subString.FindF(aLeft);
    if(pos>=0){
      do{
        subString.Set(subString.Mid(pos+aLeft.Length()));
        pos=subString.FindF(aRight);
        if(pos>=0){
          TPtrC8 temp(subString.Left(pos));
          if(MatchAll(temp,aKeys,aSplitter)){
            result.Set(temp);
            break;
          }
          if(pos+aRight.Length()>subString.Length()) break;
          subString.Set(subString.Mid(pos+aRight.Length()+1));
          pos=subString.FindF(aLeft);
        }
      }while(pos>=0);
    }
  }
  return result;
}

TPtrC8 FindEnclosed(const TDesC8& aTarget,const TDesC8& aLeft,const TDesC8& aRight,TBool aReverse){
  TPtrC8 result;
  if(aTarget.Length()>0){
    if(aReverse){//search from aRight
      TInt pos=aTarget.FindF(aRight);
      if(pos>=0){
        TPtrC8 temp=aTarget.Left(pos);
        pos=temp.FindF(aLeft);
        if(pos>=0){
          do{
            temp.Set(temp.Right(temp.Length()-pos-aLeft.Length()));
            pos=temp.FindF(aLeft);
          }while(pos>=0);
          result.Set(temp);
        }
      }
    }else{
      TInt startIndex=aTarget.FindF(aLeft);
      if(startIndex>=0){
        TPtrC8 rightPart(aTarget.Right(aTarget.Length()-startIndex-aLeft.Length()));
        TInt endIndex=rightPart.FindF(aRight);
        if(endIndex>=0){
          result.Set(rightPart.Left(endIndex));
        }
      }
    }
  }
  return result;
}

TPtrC8 FindLeft(const TDesC8& aTarget,const TDesC8& aFlag){
  TPtrC8 result;
  if(aTarget.Length()>0){
    TInt startIndex=aTarget.Find(aFlag);
    if(startIndex>=0){
      result.Set(aTarget.Left(startIndex));
    }
  }
  return result;
}
TPtrC8 FindRight(const TDesC8& aTarget,const TDesC8& aFlag){
  TPtrC8 result;
  if(aTarget.Length()>0){
    TInt startIndex=aTarget.Find(aFlag);
    if(startIndex>=0){
      result.Set(aTarget.Right(aTarget.Length()-startIndex-aFlag.Length()));
    }
  }
  return result;
}

TPtrC8 UnicodeToUtf8(const TDesC& aDes){
  TPtrC8 result;
  if(aDes.Length()>0){
    HBufC8* hBufC8=HBufC8::NewL(aDes.Length()*3);
    CleanupStack::PushL(hBufC8);
    if(NULL!=hBufC8){
      TPtr8 ptr8(hBufC8->Des());
      CnvUtfConverter::ConvertFromUnicodeToUtf8(ptr8,aDes);
      result.Set(hBufC8->Des());
    }
    CleanupStack::PopAndDestroy(hBufC8);
  }
  return result;
}

//This method is from Internet, not test yet.
void KillProcess(const TDesC& aProcessNameNotExtName){
  Log(_L8("KillProcess begin...."));
  TFullName name;
  TFindProcess finder;
  while(finder.Next(name)==KErrNone){
    if(name.FindF(aProcessNameNotExtName)!=KErrNotFound){
      Log(name);
      RProcess process;
      if(process.Open(finder,EOwnerProcess)==KErrNone){ // Open process
        process.Kill(KErrNone);
      }
      process.Close();
      break;
    }
  }
  Log(_L8("KillProcess end...."));
}

#ifdef __THIRDPARTY_PROTECTED__

TBool CreateProcess(const TDesC &aFileName){
  Log(_L8("CreateProcess begin....."));
  Log(aFileName);
  TBool result=EFalse;
  TRAPD(error,{
        RProcess *process=new RProcess;
        TUidType uidtype(KNullUid);
        TInt ifopen=process->Create(aFileName,_L(""),uidtype);
        if(KErrNone==ifopen){
          process->Resume();
          result=ETrue;
        }
        process->Close();
        delete process;
      });
  if(error!=KErrNone){
    result=EFalse;
  }
  Log(_L8("CreateProcess end"));
  return result;
}
/*
 TBool FindProcess(const TDesC &aName){
 Log(_L8("FindProcess begin...."));
 TFindProcess fp;
 TFullName procName;
 TBool result=EFalse;
 while(fp.Next(procName)==KErrNone){
 if(procName.FindF(aName)!=KErrNotFound){
 //rb.Create(procName);
 Log(procName);
 result=ETrue;
 break;
 }
 }
 Log(_L8("FindProcess end"));
 return result;
 }

 TInt StartProtected(){
 Log(_L8("StartProtected started"));
 TPtrC ptr(BlockApps);
 TInt pos=ptr.FindF(_L(","));
 TBool hasBlockedApp=EFalse;
 Log(_L8("Scan process start"));
 if(pos>=0){
 do{
 TPtrC app=ptr.Left(pos);
 if(FindProcess(app)){
 hasBlockedApp=ETrue;
 break;
 }
 ptr.Set(ptr.Mid(pos+1));
 pos=ptr.FindF(_L(","));
 }while(pos>=0);
 }
 Log(_L8("Scan process end"));
 CreateProcess(KPROTECTED_APP_NAME);
 return 0;
 }
 */
#endif
