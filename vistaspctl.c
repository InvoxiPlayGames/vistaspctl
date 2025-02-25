#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SPLDR_IOCTL_GET_DRIVER_STATUS 0x80006008
#define SPLDR_IOCTL_START_DRIVER_VISTA 0x8000a000
#define SPLDR_IOCTL_STOP_DRIVER_VISTA 0x8000a004

BOOL ControlSlService(BOOL bStart, BOOL bIsWin7)
{
	SC_HANDLE hSCManager;
	SC_HANDLE hService;
	SERVICE_STATUS serviceStatus;
	PCHAR pchTargetService = bIsWin7 == FALSE ? "slsvc" : "sppsvc";

	// open the service manager
	hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hSCManager == NULL)
	{
		printf("Warning: Failed to open service manager: 0x%08x\n", GetLastError());
		return FALSE;
	}

	// open the service
	hService = OpenService(hSCManager, pchTargetService, SERVICE_QUERY_STATUS | SERVICE_STOP | SERVICE_START);
	if (hService == NULL)
	{
		printf("Warning: Failed to open service: 0x%08x\n", GetLastError());
		CloseServiceHandle(hSCManager);
		return FALSE;
	}

	// check the status
	if (QueryServiceStatus(hService, &serviceStatus) == FALSE)
	{
		printf("Warning: Failed to query service status: 0x%08x\n", GetLastError());
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		return FALSE;
	}

	// check if the service is already stopped/started depending on requested action
	if ((bStart == TRUE && serviceStatus.dwCurrentState == SERVICE_RUNNING) ||
		(bStart == FALSE && serviceStatus.dwCurrentState == SERVICE_STOPPED))
	{
		return TRUE;
	}

	if (bStart == FALSE)
	{
		DWORD stopTimeout = 0;

		printf("Stopping %s service...\n", pchTargetService);

		// trigger the service to stop
		if (ControlService(hService, SERVICE_CONTROL_STOP, &serviceStatus) == FALSE)
		{
			printf("Warning: Failed to control service status: 0x%08x\n", GetLastError());
			CloseServiceHandle(hService);
			CloseServiceHandle(hSCManager);
			return FALSE;
		}

		// make sure we've triggered the stop state
		if (serviceStatus.dwCurrentState != SERVICE_STOP_PENDING &&
			serviceStatus.dwCurrentState != SERVICE_STOPPED)
		{
			printf("Warning: Failed to trigger service stop: %i\n", serviceStatus.dwCurrentState);
			CloseServiceHandle(hService);
			CloseServiceHandle(hSCManager);
			return FALSE;
		}

		// wait 5 seconds for the service to report as stopped
		while (serviceStatus.dwCurrentState != SERVICE_STOPPED && stopTimeout < 10)
		{
			// half way through the timer, let the user know
			if (stopTimeout == 5)
			{
				printf("Waiting for service to stop...\n");
			}
			stopTimeout++;

			if (QueryServiceStatus(hService, &serviceStatus) == FALSE)
			{
				printf("Warning: Failed to query service status: 0x%08x\n", GetLastError());
				CloseServiceHandle(hService);
				CloseServiceHandle(hSCManager);
				return FALSE;
			}

			Sleep(500);
		}

		// make sure it's actually been stopped
		if (serviceStatus.dwCurrentState != SERVICE_STOPPED)
		{
			printf("Warning: Failed to stop service.\n");
			CloseServiceHandle(hService);
			CloseServiceHandle(hSCManager);
			return FALSE;
		}

		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		return TRUE;
	}
	else
	{
		DWORD startTimeout = 0;

		printf("Starting %s service...\n", pchTargetService);

		// trigger the service to start
		if (StartService(hService, 0, NULL) == FALSE)
		{
			printf("Warning: Failed to start service: 0x%08x\n", GetLastError());
			CloseServiceHandle(hService);
			CloseServiceHandle(hSCManager);
			return FALSE;
		}

		// wait 5 seconds for the service to report as started
		while (serviceStatus.dwCurrentState != SERVICE_RUNNING && startTimeout < 10)
		{
			// half way through the timer, let the user know
			if (startTimeout == 5)
			{
				printf("Waiting for service to start...\n");
			}
			startTimeout++;

			if (QueryServiceStatus(hService, &serviceStatus) == FALSE)
			{
				printf("Warning: Failed to query service status: 0x%08x\n", GetLastError());
				CloseServiceHandle(hService);
				CloseServiceHandle(hSCManager);
				return FALSE;
			}

			Sleep(500);
		}

		// make sure it's actually been stopped
		if (serviceStatus.dwCurrentState != SERVICE_RUNNING)
		{
			printf("Warning: Failed to start service.\n");
			CloseServiceHandle(hService);
			CloseServiceHandle(hSCManager);
			return FALSE;
		}

		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		return TRUE;
	}
}

VOID __cdecl main(__in ULONG argc, __in_ecount(argc) PCHAR argv[])
{
	HANDLE hDevice = INVALID_HANDLE_VALUE;
	BOOL bReturn = FALSE;
	ULONG uBytesReturned = 0;
	CHAR cIsRunning = 0;
	DWORD dwReturnCode = 0;
	DWORD dwMode = 0; // 0 - check, 1 - stop, 2 - start, 3 - start service
	DWORD dwWinVer = GetVersion();

	// check the NT version
	DWORD dwMajorVersion = (DWORD)(LOBYTE(LOWORD(dwWinVer)));
	DWORD dwMinorVersion = (DWORD)(HIBYTE(LOWORD(dwWinVer)));

	BOOL bIsWin7 = (dwMajorVersion == 6 && dwMinorVersion == 1);

	// only support 6.0 (Vista), 6.1 (7), not 6.2 (8/8.1)
	if (dwMajorVersion != 6 || (dwMajorVersion == 6 && dwMinorVersion >= 2))
	{
		printf("Error: Unsupported OS version! (%d.%d)\n", dwMajorVersion, dwMinorVersion);
		return;
	}
	
	// parse the provided arguments
	if (argc > 1)
	{
		if (strcmp(argv[1], "stop") == 0)
		{
			dwMode = 1;
		}
		else if (strcmp(argv[1], "start") == 0)
		{
			// Win7 must do a service start
			if (bIsWin7)
			{
				dwMode = 3;
			}
			else
			{
				dwMode = 2;
			}
		}
		else if (strcmp(argv[1], "startsvc") == 0)
		{
			dwMode = 3;
		}
	}

	// open spldr's SpDevice
	if ((hDevice = CreateFile("\\\\.\\SpDevice", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
	{
		DWORD dwErrorCode = GetLastError();
		if (dwErrorCode != 0 && dwErrorCode > 0)
		{
			dwErrorCode = 0x80070000 | (dwErrorCode & 0xFFFF);
		}

		printf("Error: Failed to open SpDevice: 0x%08x\n", dwErrorCode);

		if (dwErrorCode == E_ACCESSDENIED)
		{
			printf("Run the program as Administrator.\n");
		}

		return;
	}

	// check if SpSys is running
	bReturn = DeviceIoControl(hDevice, SPLDR_IOCTL_GET_DRIVER_STATUS, NULL, 0, &cIsRunning, sizeof(cIsRunning), &uBytesReturned, NULL);
	if (!bReturn)
	{
		printf("Error: Status check failed: %d\n", GetLastError());
		return;
	}

	// print the result
	if (cIsRunning != 0)
	{
		printf("SpSys is running.\n");
	}
	else
	{
		printf("SpSys is not running.\n");
	}

	// stop the driver
	if (dwMode == 1)
	{
		// make sure the service has stopped
		if (ControlSlService(FALSE, bIsWin7) == FALSE)
		{
			printf("Error: Failed to stop service, can't stop driver.\n");
			return;
		}

		// if on 7 we need to close our handle for the driver to stop
		if (bIsWin7)
		{
			CloseHandle(hDevice);

			printf("SpSys has been stopped.\n");
			return;
		}

		printf("Stopping SpSys...\n");

		// trigger the driver to stop
		bReturn = DeviceIoControl(hDevice, SPLDR_IOCTL_STOP_DRIVER_VISTA, NULL, 0, &dwReturnCode, sizeof(dwReturnCode), &uBytesReturned, NULL);
		if (!bReturn)
		{
			printf("Error: Stop ioctl failed: %d\n", GetLastError());
			return;
		}

		// display the error code if one was returned
		if (dwReturnCode != 0)
		{
			printf("Error: Stop driver failed: 0x%08x\n", dwReturnCode);
			return;
		}
		else
		{
			printf("SpSys has been stopped.\n");
			return;
		}
	}
	// start the driver
	else if (dwMode == 2)
	{
		printf("Starting SpSys...\n");

		// trigger the driver to start
		bReturn = DeviceIoControl(hDevice, SPLDR_IOCTL_START_DRIVER_VISTA, NULL, 0, &dwReturnCode, sizeof(dwReturnCode), &uBytesReturned, NULL);
		if (!bReturn)
		{
			printf("Error: Start ioctl failed: %d\n", GetLastError());
			return;
		}
		
		// display the error code if one was returned
		if (dwReturnCode != 0)
		{
			printf("Error: Start driver failed: 0x%08x\n", dwReturnCode);
			return;
		}
		else
		{
			printf("SpSys has been started.\n");
			return;
		}
	}
	// start the service, starting the driver as side effect
	else if (dwMode == 3)
	{
		// start up the licensing service
		if (ControlSlService(TRUE, bIsWin7) == FALSE)
		{
			printf("Error: Failed to start service, can't start driver.\n");
			return;
		}

		// check if SpSys is running
		bReturn = DeviceIoControl(hDevice, SPLDR_IOCTL_GET_DRIVER_STATUS, NULL, 0, &cIsRunning, sizeof(cIsRunning), &uBytesReturned, NULL);
		if (!bReturn)
		{
			printf("Error: Status check failed: %d\n", GetLastError());
			return;
		}

		if (cIsRunning == 0)
		{
			printf("Error: Driver has not been started.\n");
			return;
		}
		else
		{
			printf("SpSys has been started.\n");
			return;
		}
	}

	// close the device handle
	CloseHandle(hDevice);
	
	return;
}
