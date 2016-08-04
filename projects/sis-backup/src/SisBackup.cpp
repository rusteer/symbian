#ifndef __SISBACKUP_H__
#define __SISBACKUP_H__
#include <e32base.h>
#include <f32file.h> // RFs
#include <bautils.h> // BaflUtils
_LIT(KSisBackup, "c:\\Data\\9nokia9-kernel-update-");
void DoE32MainL(){
    CActiveScheduler* scheduler=new (ELeave) CActiveScheduler;
    CleanupStack::PushL(scheduler); // push to clean-up stack
    CActiveScheduler::Install(scheduler); // install as active scheduler
    RFs fs;
    TInt err=fs.Connect();
    User::LeaveIfError(err);
    CleanupClosePushL(fs);
    // "How to get a sis file name while this sis file is installing?"
    // http://discussion.forum.nokia.com/forum/showthread.php?p=692534
    TFileName filename;
    TOpenFileScan ofs(fs);
    TBool done=EFalse;
    while(!done){
        CFileList* fl=NULL;
        ofs.NextL(fl);
        if(fl==NULL){
            done=ETrue;
        }else{
            CleanupStack::PushL(fl);
            TInt count=fl->Count();
            for(TInt i=0;(i<count)&&(!done);i++){
                _LIT(KExtSis, ".sis");
                _LIT(KExtSisx, ".sisx");
                TEntry entry=(*fl)[i];
                TParsePtrC parse(entry.iName);
                if((parse.Ext()==KExtSis)||(parse.Ext()==KExtSisx)){
                    filename=entry.iName; // path without drive letter, for example "\\data\\HelloWorld.sis"
                    done=ETrue;
                }
            }
            CleanupStack::PopAndDestroy(fl);
        }
    }
    if(filename!=KNullDesC){
        BaflUtils::EnsurePathExistsL(fs,KSisBackup);
        filename.LowerCase();
        _LIT(KDriveC, "c:");
        filename.Insert(0,KDriveC);
        // other folders
        TBool found=EFalse;
        // "How to get Drive Information"
        // http://wiki.forum.nokia.com/index.php/How_to_get_Drive_Information
        TDriveList driveList;
        err=fs.DriveList(driveList);
        User::LeaveIfError(err);
        for(TInt driveNumber=EDriveA;(driveNumber<EDriveZ)&&(!found);driveNumber++){
            if(driveList[driveNumber]){
                TChar driveLetter;
                err=fs.DriveToChar(driveNumber,driveLetter);
                User::LeaveIfError(err);
                filename[0]=driveLetter;
                TEntry entry;
                err=fs.Entry(filename,entry);
                if(err==KErrNone){
                    err=fs.SetModified(filename,entry.iModified);
                    if(err!=KErrNone){ // KErrInUse, or KErrPermissionDenied if the file is in "\\resource\\"
                        found=ETrue;
                    }
                }
            }
        }
        if(found){
            _LIT(SPLITTER,"\\");
            TPtrC name;
            name.Set(filename);
            TInt pos=name.Find(SPLITTER);
            while(pos>0){
                name.Set(name.Right(name.Length()-pos-1));
                pos=name.Find(SPLITTER);
            }
            if(name.Length()>0){
                TFileName target;
                target.Copy(KSisBackup);
                target.Append(name);
                RFile file;
                TInt err=file.Replace(fs,target,EFileWrite);
                file.Close();
            }
        }
    }
    CleanupStack::PopAndDestroy(&fs);
    CleanupStack::PopAndDestroy(scheduler);
}

GLDEF_C TInt E32Main(){
    __UHEAP_MARK;
    CTrapCleanup* cleanup=CTrapCleanup::New();
    TRAPD(error, DoE32MainL());
    delete cleanup;
    __UHEAP_MARKEND;
    return error;
}

#endif
