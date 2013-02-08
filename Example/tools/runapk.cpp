#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/stat.h>

using namespace std;

#define STRSTR(s) #s
#define STR(s) STRSTR(s)

static int Build();
static int Run();

int main(int argc, const char * argv[],char** envp)
{
	if( argc >= 2 && strcmp(argv[1],"build")==0 ){
		return Build();
	}
	return Run();
}

static int Run()
{
	const char *srcRoot = STR(SRCROOT);
	const char *sdkRoot = STR(ANDROID_SDK_ROOT);
	fprintf(stderr,">> [%s]\n",sdkRoot);
	if( !sdkRoot ){
		fprintf(stderr,"Error: ANDROID_SDK_ROOT is not specified\n");
		return -1;
	}
	
	//string config = getenv("CONFIGURATION");
	//for(size_t i=0;i<config.length();i++){
	//	config[i] = tolower(config[i]);
	//}
	string config("debug");
	
	const char *packageName = STR(ANDROID_PACKAGE_NAME);
	const char *activityName = STR(ANDROID_ACTIVITY_NAME);
	
	if( !packageName ){
		fprintf(stderr,"Error: CONFIGURATION is not specified\n");
		return -1;
	}
	if( !activityName ){
		fprintf(stderr,"Error: ACTIVITY is not specified\n");
		return -1;
	}
	fprintf(stderr,"Activity: %s.%s\n",packageName,activityName);
	
	string apkFile = string(srcRoot)+"/bin/"+activityName;
	if( config == "release" ){
		apkFile += "-release-unsigned.apk";
	}else if( config == "debug" ){
		apkFile += "-debug.apk";
	}else{
		fprintf(stderr,"Error: CONFIGURATION is not specified\n");
		return -1;
	}
	
	struct stat st;
	if( stat(apkFile.c_str(),&st)!=0 ){
		fprintf(stderr,"Failed to find apk [%s]\n",apkFile.c_str());
		return -1;
	}
	
	string adbPath = string(sdkRoot)+"/platform-tools/adb";
	string cmd = adbPath + " devices";
	FILE *pipe = popen(cmd.c_str(),"r");
	if( !pipe ){
		fprintf(stderr, "Error: Failed to exec 'adb devices'\n");
		return -1;
	}
	char buff[512];
	int deviceNum = 0;
	fgets(buff, 511, pipe);
	while( fgets(buff, 511, pipe) ) {
		if( strlen(buff) < 3 ){ break; }
		deviceNum++;
	}
	pclose(pipe);
	
	if( deviceNum == 0 ){
		fprintf(stderr, "Error: No android device found\n");
		return -1;
	}
	// install
	
	cmd = adbPath + " install -r \"" + apkFile + "\"";
	if( system(cmd.c_str())!=0 ){
		fprintf(stderr,"Error: Failed to install apk [%s]\n",apkFile.c_str());
		return -1;
	}
	
	cmd = adbPath + " shell am start ";
	cmd += " -a android.intent.action.MAIN";
	cmd += string("-n ") + packageName+"/."+activityName + " ";
	if( system(cmd.c_str())!=0 ){
		fprintf(stderr,"Error: Failed to launch apk [%s]\n",apkFile.c_str());
		return -1;
	}
	
	const char *logtag = getenv("LOGTAG");
	
	if( logtag ){
		printf("Logcat started for tag [%s]...\n",logtag);
		const int l = (int)strlen(logtag);
		cmd = adbPath + " logcat";
		pipe = popen(cmd.c_str(),"r");
		while( fgets(buff, 511, pipe) ) {
			if( strncmp(logtag,buff+2,l)==0 ){
				fprintf(stderr,"[%s]: %s",logtag,buff+2);
			}
		}
		pclose(pipe);
	}
	
	return 0;
}

static void WriteStringToFile(const char *path, const char *str)
{
	FILE *fp = fopen(path,"w");
	if( !fp ){ return; }
	fputs(str,fp);
	fclose(fp);
}

static int Build()
{
	//string config = getenv("CONFIGURATION");
	//for(size_t i=0;i<config.length();i++){
	//	config[i] = tolower(config[i]);
	//}
	string config("debug");
	
	const char *targetName = getenv("TARGET_NAME");
	fprintf(stderr,"Building for target [%s:%s]\n",targetName,config.c_str());
	
	if( !targetName ){
		fprintf(stderr,"Error: TARGET_NAME is not specified\n");
		return -1;
	}
	
	const char *sdkRoot = STR(ANDROID_SDK_ROOT);
	if( !sdkRoot || !sdkRoot[0] ){
		fprintf(stderr,"Error: ANDROID_SDK_ROOT is not specified\n");
		return -1;
	}
	
	const char *pkgName = STR(ANDROID_PACKAGE_NAME);
	if( !pkgName || !pkgName[0] ){
		fprintf(stderr,"Error: ANDROID_PACKAGE_NAME is not specified\n");
		return -1;
	}
	
	struct stat st;
	
	if( stat("./jni",&st)!=0 ){
		mkdir("./jni",0755);
		string con =  string("LOCAL_PATH := $(call my-dir)\n")+
		"include $(CLEAR_VARS) \n \n"+
		"LOCAL_MODULE    := "+targetName+"\n"+
		"LOCAL_SRC_FILES := "+targetName+".cpp\n\n"+
		"include $(BUILD_SHARED_LIBRARY)\n";
		WriteStringToFile("./jni/Android.mk",con.c_str());
		string cppFile = string("./jni/")+targetName+".cpp";
		WriteStringToFile(cppFile.c_str(),"#include<jni.h>\n");
	}
	
	const char *ndkRoot = STR(ANDROID_NDK_ROOT);
	if( ndkRoot && stat("./jni/Android.mk",&st)==0 ){
		string cmd = string(ndkRoot) + "/ndk-build";
		system(cmd.c_str());
	}
	
	if( stat("./AndroidManifest.xml",&st)!=0 ){
		printf("Initializing project [%s]\n",targetName);
		string cmd = sdkRoot;
		cmd += "/tools/android create project";
		cmd += " --target 5";
		cmd += string(" --path .");
		cmd += string(" --package ") + pkgName;
		cmd += string(" --activity ") + targetName + "Activity";
		system(cmd.c_str());
	}else{
		string cmd = sdkRoot;
		cmd += "/tools/android update project --path .";
		system(cmd.c_str());
	}
	
	if( stat("./.project",&st)!=0 ){
		string con = string("<?xml version=\"1.0\" encoding=\"UTF-8\"?>")+
			"<projectDescription>"+
			"<name>"+targetName+"</name>"
			"<comment></comment>"
			"<projects></projects>"
			"<buildSpec>"
			"<buildCommand>"
			"<name>com.android.ide.eclipse.adt.ResourceManagerBuilder</name>"
			"<arguments></arguments>"
			"</buildCommand>"
			"<buildCommand>"
			"<name>com.android.ide.eclipse.adt.PreCompilerBuilder</name>"
			"<arguments></arguments>"
			"</buildCommand>"
			"<buildCommand>"
			"<name>org.eclipse.jdt.core.javabuilder</name>"
			"<arguments></arguments>"
			"</buildCommand>"
			"<buildCommand>"
			"<name>com.android.ide.eclipse.adt.ApkBuilder</name>"
			"<arguments></arguments>"
			"</buildCommand>"
			"</buildSpec>"
			"<natures>"
			"<nature>com.android.ide.eclipse.adt.AndroidNature</nature>"
			"<nature>org.eclipse.jdt.core.javanature</nature>"
			"</natures>"
			"</projectDescription>";
		WriteStringToFile("./.project",con.c_str());
	}
	
	if( stat("./.project",&st)!=0 ){
		string con = string("<?xml version=\"1.0\" encoding=\"UTF-8\"?>")+
		"<projectDescription>"+
		"<name>"+targetName+"</name>"
		"<comment></comment>"
		"<projects></projects>"
		"<buildSpec>"
		"<buildCommand>"
		"<name>com.android.ide.eclipse.adt.ResourceManagerBuilder</name>"
		"<arguments></arguments>"
		"</buildCommand>"
		"<buildCommand>"
		"<name>com.android.ide.eclipse.adt.PreCompilerBuilder</name>"
		"<arguments></arguments>"
		"</buildCommand>"
		"<buildCommand>"
		"<name>org.eclipse.jdt.core.javabuilder</name>"
		"<arguments></arguments>"
		"</buildCommand>"
		"<buildCommand>"
		"<name>com.android.ide.eclipse.adt.ApkBuilder</name>"
		"<arguments></arguments>"
		"</buildCommand>"
		"</buildSpec>"
		"<natures>"
		"<nature>com.android.ide.eclipse.adt.AndroidNature</nature>"
		"<nature>org.eclipse.jdt.core.javanature</nature>"
		"</natures>"
		"</projectDescription>";
		WriteStringToFile("./.project",con.c_str());
	}
	
	if( stat("./.classpath",&st)!=0 ){
		string con = string("<?xml version=\"1.0\" encoding=\"UTF-8\"?>")+
		"<classpath>"
		"<classpathentry kind=\"src\" path=\"src\"/>"
		"<classpathentry kind=\"src\" path=\"gen\"/>"
		"<classpathentry kind=\"con\" path=\"com.android.ide.eclipse.adt.ANDROID_FRAMEWORK\"/>"
		"<classpathentry kind=\"con\" path=\"com.android.ide.eclipse.adt.LIBRARIES\"/>"
		"<classpathentry kind=\"output\" path=\"bin/classes\"/>"
		"</classpath>";
		WriteStringToFile("./.classpath",con.c_str());
	}
	
	string cmd = "ant ";
	cmd += config;
	fprintf(stderr,"ant running...\n");
	system(cmd.c_str());
	
	return 0;
}
