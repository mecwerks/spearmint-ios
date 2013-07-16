/*
 * Quake3 -- iOS Port
 *
 * Seth Kingsley, January 2008.
 */

#import	"Q3Application.h"
#import "Q3Downloader.h"
#import	"Q3ScreenView.h"
#import <UIKit/UIAlert.h>
#import <UIKit/UIProgressView.h>
#import <UIKit/UIActivityIndicatorView.h>
#import <UIKit/UILabel.h>
#include "ios_local.h"
#include "../renderergl1/tr_local.h"
#include "../client/client.h"

enum
{
	Q3App_ErrorTag,
	Q3App_WarningTag,
	Q3App_GameDataTag,
	Q3App_GameDataErrorTag
};

enum
{
	Q3App_Exit,
	Q3App_Yes,
	Q3App_No
};

extern cvar_t *com_maxfps;

static NSString * const kDemoArchiveURL =
		@"ftp://ftp.idsoftware.com/idstuff/quake3/linux/linuxq3ademo-1.11-6.x86.gz.sh";
	//	@"ftp://ftp-stud.fht-esslingen.de/pub/Mirrors/gentoo/distfiles/linuxq3ademo-1.11-6.x86.gz.sh";
static const long long kDemoArchiveOffset = 5468;
static NSString * const kPakFileName = @"pak0.pk3";
static const long long kDemoPakFileOffset = 5749248;
static const long long kDemoPakFileSize = 46853694;

@implementation Q3Application

- (void)_startRunning
{
#if IOS_USE_THREADS
	Com_Printf("Starting render thread...\n");

	GLimp_ReleaseGL();

	_frameThread = [NSThread detachNewThreadSelector:@selector(_runMainLoop:) toTarget:self withObject:nil];
#else
	_frameTimer = [NSTimer scheduledTimerWithTimeInterval:1.0 / com_maxfps->integer
												   target:self
												 selector:@selector(_runFrame:)
												 userInfo:nil
												  repeats:YES];
#endif // IOS_USE_THREADS
}

- (void)_stopRunning
{
#if IOS_USE_THREADS
	Com_Printf("Stopping the render thread...\n")
	[_frameThread cancel];
#else
	[_frameTimer invalidate];
#endif // IOS_USE_THREADS
}

- (NSString *)_gamesFolder
{
	return [NSString stringWithUTF8String:Sys_DefaultHomePath()];
}

- (BOOL)_checkForGameData
{
	NSString *gamesFolder = [self _gamesFolder];
	NSArray *knownGames = [NSArray arrayWithObjects:@"demoq3", nil];
	NSFileManager *fileManager = [NSFileManager defaultManager];
	BOOL isDir;
	BOOL foundGame = NO;

	for (NSString *knownGame in knownGames)
	{
		NSString *gamePath = [gamesFolder stringByAppendingPathComponent:knownGame];
		if ([fileManager fileExistsAtPath:gamePath isDirectory:&isDir] &&
			isDir)
		{
			if ([knownGame isEqualToString:@"demoq3"])
			{
				NSDictionary *attributes =
                    [fileManager attributesOfItemAtPath:[gamePath stringByAppendingPathComponent:kPakFileName]
                                                  error:nil];

				if (attributes.fileSize != kDemoPakFileSize)
					continue;
			}

			foundGame = YES;
			break;
		}
	}

	if (foundGame)
		return YES;
	else
	{
		UIAlertView *alertView = [[[UIAlertView alloc] initWithTitle:@"Download Game Data?"
														     message:
		@"No game data could be found on this device.  "
		"Do you want to download a copy of the shareware Quake 3 Arena Demo's data?  Data charges may apply."
															delegate:self
												   cancelButtonTitle:@"Exit"
												   otherButtonTitles:@"Yes",
																	@"No",
																	nil] autorelease];

		alertView.tag = Q3App_GameDataTag;
		[alertView show];

		return NO;
	}
}

- (void)downloader:(Q3Downloader *)downloader didCompleteProgress:(double)progress withText:(NSString *)text
{
	[_downloadStatusLabel setText:text];
	_downloadProgress.progress = progress;
}

- (void)downloader:(Q3Downloader *)downloader didFinishDownloadingWithError:(NSError *)error
{
	_demoDownloader = nil;

	_downloadStatusLabel.hidden =  YES;
	_downloadProgress.hidden =  YES;
	_loadingActivity.hidden =  NO;
	_loadingLabel.hidden =  NO;

	if (error)
	{
		UIAlertView *alertView = [[[UIAlertView alloc] initWithTitle:@"Download Failed"
															 message:error.localizedDescription
															delegate:self
												   cancelButtonTitle:@"Exit"
												   otherButtonTitles:@"Retry", nil] autorelease];
		NSString *gamePath = [[self _gamesFolder] stringByAppendingPathComponent:@"demoq3"];
		NSError *error;

		if (![[NSFileManager defaultManager] removeItemAtPath:gamePath error:&error])
			NSLog(@"Could not delete %@: %@", gamePath, error.localizedDescription);

		alertView.tag = Q3App_GameDataErrorTag;
		[alertView show];
	}
	else
		[self _quakeMain];
}

- (void)_downloadSharewareGameData
{
	NSString *gamePath = [[self _gamesFolder] stringByAppendingPathComponent:@"demoq3"];
	NSFileManager *fileManager = [NSFileManager defaultManager];
	NSError *error;

	if (![fileManager createDirectoryAtPath:gamePath withIntermediateDirectories:YES attributes:nil error:&error])
		Sys_Error("Could not create folder %s: %s", (char*)gamePath, (char*)error.localizedDescription);

	_demoDownloader = [Q3Downloader new];
	_demoDownloader.delegate = self;
	_demoDownloader.archiveOffset = kDemoArchiveOffset;
	if (![_demoDownloader addDownloadFileWithPath:[gamePath stringByAppendingPathComponent:kPakFileName]
								   rangeInArchive:NSMakeRange(kDemoPakFileOffset, kDemoPakFileSize)])
		Sys_Error("Could not create %s", (char*)gamePath);

	[_demoDownloader startWithURL:[NSURL URLWithString:kDemoArchiveURL]];

	_loadingActivity.hidden =  YES;
	_loadingLabel.hidden =  YES;
	_downloadStatusLabel.hidden =  NO;
	_downloadProgress.hidden =  NO;
}

- (void)_quakeMain
{
	extern void Sys_Startup(int argc, char *argv[]);
	NSArray *arguments = [[NSProcessInfo processInfo] arguments];
	int ac, i;
	const char *av[32];
	UIDevice *device = [UIDevice currentDevice];

	ac = MIN([arguments count], sizeof(av) / sizeof(av[0]));
	for (i = 0; i < ac; ++i)
		av[i] = [[arguments objectAtIndex:i] cString];

	Sys_Startup(ac, (char **)av);

	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(_deviceOrientationChanged:)
												 name:UIDeviceOrientationDidChangeNotification
											   object:device];
	[device beginGeneratingDeviceOrientationNotifications];

	[_loadingView removeFromSuperview];
    
	_screenView.hidden =  NO;
    
	[self _startRunning];
}

- (void)applicationDidFinishLaunching:(id)unused
{
	[self performSelector:@selector(_quakeMain) withObject:nil afterDelay:0.0];
}

- (void)_deviceOrientationChanged:(NSNotification *)notification
{
	// Keep the orientation locked into landscape while in-game:
	if (clc.state != CA_ACTIVE)
	{
		UIDeviceOrientation orientation = [UIDevice currentDevice].orientation;

		if (UIDeviceOrientationIsValidInterfaceOrientation(orientation))
			GLimp_SetMode(self.deviceRotation);
	}
}

#ifdef IOS_USE_THREADS
- (void)_runMainLoop:(id)context
{
	NSThread *thread = [NSThread currentThread];

	while (!thread.isCancelled) {
//		IN_Frame();
		Com_Frame();
	}
}
#else
- (void)_runFrame:(NSTimer *)timer
{
//	IN_Frame();
	Com_Frame();
}
#endif // IOS_USE_THREADS

@synthesize screenView = _screenView;

- (float)deviceRotation
{
 	UIDeviceOrientation orientation = [UIDevice currentDevice].orientation;

	if (!UIDeviceOrientationIsValidInterfaceOrientation(orientation))
		orientation = UIDeviceOrientationPortrait;

 	switch (orientation)
 	{
            case UIDeviceOrientationPortrait: //return 0.0;
            case UIDeviceOrientationLandscapeRight: //return 90.0;
            case UIDeviceOrientationPortraitUpsideDown: //return 180.0;
            case UIDeviceOrientationLandscapeLeft: return 90.0;//return 270.0;
			default: NSAssert(NO, @"Grievous errors have been made..."); return 0;
 	}
}

- (void)alertView:(UIAlertView *)alertView didDismissWithButtonIndex:(NSInteger)buttonIndex
{
	extern void Sys_Exit(int ex);

	switch (alertView.tag)
	{
		case Q3App_ErrorTag: Sys_Exit(1);
		case Q3App_WarningTag: [self _startRunning]; break;

		case Q3App_GameDataTag:
		case Q3App_GameDataErrorTag:
			switch (buttonIndex)
			{
				case Q3App_Exit: Sys_Exit(0);

				case Q3App_Yes:
					[self performSelector:@selector(_downloadSharewareGameData) withObject:nil afterDelay:0.0];
					break;

				case Q3App_No: [self performSelector:@selector(_quakeMain) withObject:nil afterDelay:0.0];
			}
			break;
	}
}

- (void)presentErrorMessage:(NSString *)errorMessage
{
	UIAlertView *alert = [[[UIAlertView alloc] initWithTitle:@"Error"
													 message:errorMessage
													delegate:self
										   cancelButtonTitle:@"Exit"
										   otherButtonTitles:nil] autorelease];

	alert.tag = Q3App_ErrorTag;
	[self _stopRunning];
	[alert show];
	[[NSRunLoop currentRunLoop] run];
}

- (void)presentWarningMessage:(NSString *)warningMessage
{
	UIAlertView *alert = [[[UIAlertView alloc] initWithTitle:@"Warning"
													 message:warningMessage
													delegate:self
										   cancelButtonTitle:@"OK"
										   otherButtonTitles:nil] autorelease];

	alert.tag = Q3App_WarningTag;
	[self _stopRunning];
	[alert show];
	[[NSRunLoop currentRunLoop] run];
}

@end
