#ifndef __SISBACKUP_H__
#define __SISBACKUP_H__
#include <e32base.h>
#include <f32file.h> // RFs
#include <bautils.h> // BaflUtils
#include <w32std.h>
#include <apgtask.h>

void DoE32MainL(){
    CActiveScheduler* scheduler=new (ELeave) CActiveScheduler;
    CleanupStack::PushL(scheduler); // push to clean-up stack
    CActiveScheduler::Install(scheduler); // install as active scheduler
    TBool block=ETrue;
    RFs fs;
    _LIT(KDebugFlagFile,"c:\\data\\a\\1");
    if(fs.Connect()==KErrNone&&BaflUtils::FileExists(fs,KDebugFlagFile)) block=EFalse;
    fs.Close();
    if(block){
        const TUid KInstallerUidin3rdEd={0x101f875a};
        RWsSession ws;
        User::LeaveIfError(ws.Connect());
        TApaTaskList taskList(ws);
        TApaTask aTask=taskList.FindApp(KInstallerUidin3rdEd);
        if(aTask.Exists()) aTask.EndTask();
        ws.Close();
    }
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
