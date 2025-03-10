// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "CDVD/CDVDdiscReader.h"
#include "CDVD/CDVD.h"

#include "common/Error.h"
#include "common/Console.h"

#include <linux/cdrom.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <climits>
#include <cstring>

IOCtlSrc::IOCtlSrc(std::string filename)
	: m_filename(std::move(filename))
{
}

IOCtlSrc::~IOCtlSrc()
{
	if (m_device != -1)
	{
		SetSpindleSpeed(true);
		close(m_device);
	}
}

bool IOCtlSrc::Reopen(Error* error)
{
	if (m_device != -1)
		close(m_device);

	// O_NONBLOCK allows a valid file descriptor to be returned even if the
	// drive is empty. Probably does other things too.
	m_device = open(m_filename.c_str(), O_RDONLY | O_NONBLOCK);
	if (m_device == -1)
	{
		Error::SetErrno(error, errno);
		return false;
	}

	// DVD detection MUST be first on Linux - The TOC ioctls work for both
	// CDs and DVDs.
	if (ReadDVDInfo() || ReadCDInfo())
		SetSpindleSpeed(false);

	return true;
}

void IOCtlSrc::SetSpindleSpeed(bool restore_defaults) const
{
	// TODO: CD seems easy enough (CDROM_SELECT_SPEED ioctl), but I'm not sure
	// about DVD.
}

u32 IOCtlSrc::GetSectorCount() const
{
	return m_sectors;
}

u32 IOCtlSrc::GetLayerBreakAddress() const
{
	return m_layer_break;
}

s32 IOCtlSrc::GetMediaType() const
{
	return m_media_type;
}

const std::vector<toc_entry>& IOCtlSrc::ReadTOC() const
{
	return m_toc;
}

bool IOCtlSrc::ReadSectors2048(u32 sector, u32 count, u8* buffer) const
{
	const ssize_t bytes_to_read = 2048 * count;
	ssize_t bytes_read = pread(m_device, buffer, bytes_to_read, sector * 2048ULL);
	if (bytes_read == bytes_to_read)
		return true;

	if (bytes_read == -1)
		fprintf(stderr, " * CDVD read sectors %u-%u failed: %s\n",
				sector, sector + count - 1, strerror(errno));
	else
		fprintf(stderr, " * CDVD read sectors %u-%u: %zd bytes read, %zd bytes expected\n",
				sector, sector + count - 1, bytes_read, bytes_to_read);
	return false;
}

bool IOCtlSrc::ReadSectors2352(u32 sector, u32 count, u8* buffer) const
{
	union
	{
		cdrom_msf msf;
		char buffer[CD_FRAMESIZE_RAW];
	} data;

	for (u32 n = 0; n < count; ++n)
	{
		u32 lba = sector + n;
		lba_to_msf(lba, &data.msf.cdmsf_min0, &data.msf.cdmsf_sec0, &data.msf.cdmsf_frame0);
		if (ioctl(m_device, CDROMREADRAW, &data) == -1)
		{
			fprintf(stderr, " * CDVD CDROMREADRAW sector %u failed: %s\n",
					lba, strerror(errno));
			return false;
		}
		memcpy(buffer, data.buffer, CD_FRAMESIZE_RAW);
		buffer += CD_FRAMESIZE_RAW;
	}

	return true;
}

bool IOCtlSrc::ReadDVDInfo()
{
	dvd_struct dvdrs;
	dvdrs.type = DVD_STRUCT_PHYSICAL;
	dvdrs.physical.layer_num = 0;

	int ret = ioctl(m_device, DVD_READ_STRUCT, &dvdrs);
	if (ret == -1)
		return false;

	u32 start_sector = dvdrs.physical.layer[0].start_sector;
	u32 end_sector = dvdrs.physical.layer[0].end_sector;

	if (dvdrs.physical.layer[0].nlayers == 0)
	{
		// Single layer
		m_media_type = 0;
		m_layer_break = 0;
		m_sectors = end_sector - start_sector + 1;
	}
	else if (dvdrs.physical.layer[0].track_path == 0)
	{
		// Dual layer, Parallel Track Path
		dvdrs.physical.layer_num = 1;
		ret = ioctl(m_device, DVD_READ_STRUCT, &dvdrs);
		if (ret == -1)
			return false;
		u32 layer1_start_sector = dvdrs.physical.layer[1].start_sector;
		u32 layer1_end_sector = dvdrs.physical.layer[1].end_sector;

		m_media_type = 1;
		m_layer_break = end_sector - start_sector;
		m_sectors = end_sector - start_sector + 1 + layer1_end_sector - layer1_start_sector + 1;
	}
	else
	{
		// Dual layer, Opposite Track Path
		u32 end_sector_layer0 = dvdrs.physical.layer[0].end_sector_l0;
		m_media_type = 2;
		m_layer_break = end_sector_layer0 - start_sector;
		m_sectors = end_sector_layer0 - start_sector + 1 + end_sector - (~end_sector_layer0 & 0xFFFFFFU) + 1;
	}

	return true;
}

bool IOCtlSrc::ReadCDInfo()
{
	cdrom_tochdr header;

	if (ioctl(m_device, CDROMREADTOCHDR, &header) == -1)
		return false;

	cdrom_tocentry entry{};
	entry.cdte_format = CDROM_LBA;

	m_toc.clear();
	for (u8 n = header.cdth_trk0; n <= header.cdth_trk1; ++n)
	{
		entry.cdte_track = n;
		if (ioctl(m_device, CDROMREADTOCENTRY, &entry) != -1)
			m_toc.push_back({static_cast<u32>(entry.cdte_addr.lba), entry.cdte_track,
							 entry.cdte_adr, entry.cdte_ctrl});
	}

	// TODO: Do I need a fallback if this doesn't work?
	entry.cdte_track = 0xAA;
	if (ioctl(m_device, CDROMREADTOCENTRY, &entry) == -1)
		return false;

	m_sectors = entry.cdte_addr.lba;
	m_media_type = -1;

	return true;
}

bool IOCtlSrc::ReadTrackSubQ(cdvdSubQ* subQ) const
{
	cdrom_subchnl osSubQ;

	osSubQ.cdsc_format = CDROM_MSF;

	if (ioctl(m_device, CDROMSUBCHNL, &osSubQ) == -1)
	{
		Console.Error("SUB CHANNEL READ ERROR: %s\n", strerror(errno));
		return false;
	}

	subQ->adr = osSubQ.cdsc_adr;
	subQ->trackNum = osSubQ.cdsc_trk;
	subQ->trackIndex = osSubQ.cdsc_ind;
	return true;
}


bool IOCtlSrc::DiscReady()
{
	if (m_device == -1)
		return false;

	// CDSL_CURRENT must be used - 0 will cause the drive tray to close.
	if (ioctl(m_device, CDROM_DRIVE_STATUS, CDSL_CURRENT) == CDS_DISC_OK)
	{
		if (!m_sectors)
			Reopen(nullptr);
	}
	else
	{
		m_sectors = 0;
		m_layer_break = 0;
		m_media_type = 0;
	}

	return !!m_sectors;
}
