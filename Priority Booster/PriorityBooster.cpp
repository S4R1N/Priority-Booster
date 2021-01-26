#include <ntifs.h>
#include <ntddk.h>

#include "PriorityBoosterCommon.h"

UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\PriorityBooster");


// proto

void PriorityBoosterUnload(_In_ PDRIVER_OBJECT DriverObject);
NTSTATUS PriorityBoosterCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
extern "C" NTSTATUS PriorityBoosterDeviceControl(PDEVICE_OBJECT, PIRP Irp);

// da shit.

extern "C" NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	
	UNREFERENCED_PARAMETER(RegistryPath);

	DriverObject->DriverUnload = PriorityBoosterUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = PriorityBoosterCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = PriorityBoosterCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = PriorityBoosterDeviceControl;

	PDEVICE_OBJECT DeviceObject;
	NTSTATUS status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
	if (!NT_SUCCESS(status)) {

		KdPrint(("Failed to create device object (0x%08X)\n", status));
		return status;

	}

	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\PriorityBooster");
	status = IoCreateSymbolicLink(&symLink, &devName);
	if (!NT_SUCCESS(status)) {

		KdPrint(("Failed to create symboloic link (0x%08X)\n", status));
		IoDeleteDevice(DeviceObject);
		return status;

	}

	return STATUS_SUCCESS;

}

void PriorityBoosterUnload(_In_ PDRIVER_OBJECT DriverObject) {

	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\PriorityBooster");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);

}


NTSTATUS PriorityBoosterCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp) {

	UNREFERENCED_PARAMETER(DeviceObject);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	
	return STATUS_SUCCESS;

}


extern "C" NTSTATUS PriorityBoosterDeviceControl(PDEVICE_OBJECT, PIRP Irp) {

	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto status = STATUS_SUCCESS;

	switch (stack->Parameters.DeviceIoControl.IoControlCode) {

		case (ULONG)IOCTL_PRIORITY_BOOSTER_SET_PRIORITY: {

			if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ThreadData)) {

				status = STATUS_BUFFER_TOO_SMALL;
				break;

			}

			auto data = (ThreadData*)stack->Parameters.DeviceIoControl.Type3InputBuffer;
			if (data == nullptr) {

				status = STATUS_INVALID_PARAMETER;
				break;

			}
			if (data->Priority < 1 || data->Priority > 31) {

				status = STATUS_INVALID_PARAMETER;
				break;
			}

			PETHREAD Thread;
			status = PsLookupThreadByThreadId(ULongToHandle(data->ThreadId), &Thread);
			if (!NT_SUCCESS(status))
				break;

			KeSetPriorityThread((PKTHREAD)Thread, data->Priority);

			ObDereferenceObject(Thread);
			KdPrint(("Thread Priority change for %d to %d succeeded\n", data->ThreadId, data->Priority));

			break;

		}
		default: {
			status = STATUS_INVALID_DEVICE_REQUEST;
			break;
		}

	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return status;

}