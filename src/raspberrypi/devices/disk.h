//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//
//	XM6i
//	Copyright (C) 2010-2015 isaki@NetBSD.org
//
//  	Imported sava's Anex86/T98Next image and MO format support patch.
//  	Comments translated to english by akuker.
//
//	[ Disk ]
//
//---------------------------------------------------------------------------

#pragma once

#include "log.h"
#include "scsi.h"
#include "controllers/scsidev_ctrl.h"
#include "device.h"
#include "device_factory.h"
#include "disk_track_cache.h"
#include "file_support.h"
#include "filepath.h"
#include <string>
#include <set>
#include <map>

#include "../rascsi.h"
#include "interfaces/scsi_block_commands.h"
#include "interfaces/scsi_primary_commands.h"

class Disk : public Device, public ScsiPrimaryCommands, public ScsiBlockCommands
{
private:
	enum access_mode { RW6, RW10, RW16 };

	// The supported configurable block sizes, empty if not configurable
	set<uint32_t> sector_sizes;
	uint32_t configured_sector_size;

	// The mapping of supported capacities to block sizes and block counts, empty if there is no capacity restriction
	map<uint64_t, Geometry> geometries;

	SASIDEV::ctrl_t *ctrl;

protected:
	// Internal data structure
	typedef struct {
		uint32_t size;							// Sector Size (8=256, 9=512, 10=1024, 11=2048, 12=4096)
		// TODO blocks should be a 64 bit value in order to support higher capacities
		uint32_t blocks;						// Total number of sectors
		DiskCache *dcache;						// Disk cache
		off_t imgoffset;						// Offset to actual data
	} disk_t;

private:
	typedef struct _command_t {
		const char* name;
		void (Disk::*execute)(SASIDEV *);

		_command_t(const char* _name, void (Disk::*_execute)(SASIDEV *)) : name(_name), execute(_execute) { };
	} command_t;
	std::map<SCSIDEV::scsi_command, command_t*> commands;

	void AddCommand(SCSIDEV::scsi_command, const char*, void (Disk::*)(SASIDEV *));

public:
	// Basic Functions
	Disk(std::string);							// Constructor
	virtual ~Disk();							// Destructor

	// Media Operations
	virtual void Open(const Filepath& path);	// Open
	void GetPath(Filepath& path) const;				// Get the path
	bool Eject(bool) override;					// Eject
	bool Flush();							// Flush the cache

	// Commands covered by the SCSI specification
	virtual void TestUnitReady(SASIDEV *) override;
	void Inquiry(SASIDEV *) override;
	void RequestSense(SASIDEV *) override;
	void ModeSelect6(SASIDEV *);
	void ModeSelect10(SASIDEV *);
	void ModeSense6(SASIDEV *);
	void ModeSense10(SASIDEV *);
	void Rezero(SASIDEV *);
	void FormatUnit(SASIDEV *) override;
	void ReassignBlocks(SASIDEV *);
	void StartStopUnit(SASIDEV *);
	void SendDiagnostic(SASIDEV *);
	void PreventAllowMediumRemoval(SASIDEV *);
	void SynchronizeCache10(SASIDEV *);
	void SynchronizeCache16(SASIDEV *);
	void ReadDefectData10(SASIDEV *);
	virtual void Read6(SASIDEV *);
	void Read10(SASIDEV *) override;
	void Read16(SASIDEV *) override;
	virtual void Write6(SASIDEV *);
	void Write10(SASIDEV *) override;
	void Write16(SASIDEV *) override;
	void Verify10(SASIDEV *) override;
	void Verify16(SASIDEV *) override;
	void Seek(SASIDEV *);
	void Seek6(SASIDEV *);
	void Seek10(SASIDEV *);
	void ReadCapacity10(SASIDEV *) override;
	void ReadCapacity16(SASIDEV *) override;
	void ReportLuns(SASIDEV *) override;
	void Reserve6(SASIDEV *);
	void Reserve10(SASIDEV *);
	void Release6(SASIDEV *);
	void Release10(SASIDEV *);

	// Command helpers
	virtual int Inquiry(const DWORD *cdb, BYTE *buf) = 0;	// INQUIRY command
	virtual int WriteCheck(DWORD block);					// WRITE check
	virtual bool Write(const DWORD *cdb, const BYTE *buf, DWORD block);			// WRITE command
	bool Assign(const DWORD *cdb);					// ASSIGN command
	bool StartStop(const DWORD *cdb);				// START STOP UNIT command
	bool SendDiag(const DWORD *cdb);				// SEND DIAGNOSTIC command
	bool Removal(const DWORD *cdb);				// PREVENT/ALLOW MEDIUM REMOVAL command

	virtual int Read(const DWORD *cdb, BYTE *buf, uint64_t block);
	int ReadDefectData10(const DWORD *cdb, BYTE *buf);		// READ DEFECT DATA(10) command
	int SelectCheck(const DWORD *cdb);				// SELECT check
	int SelectCheck10(const DWORD *cdb);				// SELECT(10) check

	uint32_t GetSectorSizeInBytes() const;
	void SetSectorSizeInBytes(uint32_t, bool);
	uint32_t GetSectorSize() const;
	bool IsSectorSizeConfigurable() const;
	set<uint32_t> GetSectorSizes() const;
	void SetSectorSizes(const set<uint32_t>&);
	uint32_t GetConfiguredSectorSize() const;
	bool SetConfiguredSectorSize(uint32_t);
	void SetGeometries(const map<uint64_t, Geometry>&);
	void SetGeometryForCapacity(uint64_t);
	uint64_t GetBlockCount() const;
	void SetBlockCount(uint32_t);
	bool GetStartAndCount(SASIDEV *, uint64_t&, uint32_t&, access_mode);

	// TODO Try to get rid of this method, which is called by SASIDEV (but must not)
	virtual bool ModeSelect(const DWORD *cdb, const BYTE *buf, int length);// MODE SELECT command

	virtual bool Dispatch(SCSIDEV *) override;

protected:
	// Internal processing
	virtual int AddError(bool change, BYTE *buf);			// Add error
	virtual int AddFormat(bool change, BYTE *buf);			// Add format
	virtual int AddDrive(bool change, BYTE *buf);			// Add drive
	int AddOpt(bool change, BYTE *buf);				// Add optical
	int AddCache(bool change, BYTE *buf);				// Add cache
	int AddCDROM(bool change, BYTE *buf);				// Add CD-ROM
	int AddCDDA(bool, BYTE *buf);				// Add CD_DA
	virtual int AddVendor(int page, bool change, BYTE *buf);	// Add vendor special info
	bool CheckReady();						// Check if ready
	virtual int RequestSense(const DWORD *cdb, BYTE *buf);		// REQUEST SENSE command

	// Internal data
	disk_t disk;								// Internal disk data

private:
	void Read(SASIDEV *, uint64_t);
	void Write(SASIDEV *, uint64_t);
	void Verify(SASIDEV *, uint64_t);
	bool Format(const DWORD *cdb);
	int ModeSense6(const DWORD *cdb, BYTE *buf);
	int ModeSense10(const DWORD *cdb, BYTE *buf);
};
