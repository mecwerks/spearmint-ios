/*
 * Quake3 -- iOS Port
 *
 * Seth Kingsley, January 2008.
 */

#import	"ios_local.h"

#import	"Q3Application.h"


qboolean Sys_LowPhysicalMemory(void) {
	return qtrue;
}

void Sys_Error(const char *error, ...) {
	extern void Sys_Exit(int ex) __attribute__ ((noreturn));
	NSString *errorString;
	va_list ap;

	va_start(ap, error);
	errorString = [[[NSString alloc] initWithFormat:[NSString stringWithCString:error encoding:NSUTF8StringEncoding]
																				arguments:ap] autorelease];
	va_end(ap);
#ifdef IOS_USE_THREADS
	[[Q3Application sharedApplication] performSelectorOnMainThread:@selector(presentErrorMessage:)
																											withObject:errorString
																									 waitUntilDone:YES];
#else
	[(Q3Application *)[Q3Application sharedApplication] presentErrorMessage:errorString];
#endif // IOS_USE_THREADS
	
	Sys_Exit(3);
}

void Sys_Warn( const char *warning, ...) {
	NSString *warningString;
	va_list ap;

	va_start(ap, warning);
	warningString = [[[NSString alloc] initWithFormat:[NSString stringWithCString:warning encoding:NSUTF8StringEncoding]
																					arguments:ap] autorelease];
	va_end(ap);
#ifdef IOS_USE_THREADS
	[[Q3Application sharedApplication] performSelectorOnMainThread:@selector(presentWarningMessage:)
                                                        withObject:warningString waitUntilDone:YES];
#else
	[(Q3Application *)[Q3Application sharedApplication] presentWarningMessage:warningString];
#endif // IOS_USE_THREADS
}

int main(int ac, char *av[]) {
	NSAutoreleasePool *pool = [NSAutoreleasePool new];
	UIApplicationMain(ac, av, nil, nil);

	[pool release];
	return 0;
}
