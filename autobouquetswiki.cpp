#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>
#include <sys/stat.h>

#include <string.h>
#include <unistd.h>
#include <dirent.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>
#include <list>

#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>

/*
	get rid of DMX_SET_SOURCE patch dmx.h v4.17
	id=13adefbe9e566c6db91579e4ce17f1e5193d6f2c
*/
#ifndef DMX_SET_SOURCE
typedef enum dmx_source {
	DMX_SOURCE_FRONT0 = 0,
	DMX_SOURCE_FRONT1,
	DMX_SOURCE_FRONT2,
	DMX_SOURCE_FRONT3,
	DMX_SOURCE_DVR0   = 16,
	DMX_SOURCE_DVR1,
	DMX_SOURCE_DVR2,
	DMX_SOURCE_DVR3
} dmx_source_t;
#define DMX_SET_SOURCE	_IOW('o', 49, dmx_source_t)
#endif

#include "wiki.inc"
#include "icon.inc"

#define DVB_BUFFER_SIZE 2*4096
#define DATA_SIZE 270

using namespace std;

template <class T>
string to_string(T t, ios_base & (*f)(ios_base&))
{
  ostringstream oss;
  oss << f << t;
  return oss.str();
}

u_int32_t crc_table[256] = {
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
	0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
	0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
	0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
	0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
	0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
	0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
	0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
	0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
	0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
	0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
	0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
	0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
	0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
	0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
	0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
	0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
	0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
	0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
	0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
	0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
	0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
	0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
	0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
	0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
	0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
	0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
	0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
	0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
	0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
	0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
	0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
	0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
	0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
	0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
	0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
	0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
	0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
	0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
	0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
	0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4};

u_int32_t crc32 (const char *d, int len, u_int32_t crc) {
	register int i;
	const unsigned char *u=(unsigned char*)d;

	for (i=0; i<len; i++)
		crc = (crc << 8) ^ crc_table[((crc >> 24) ^ *u++)];

	return crc;
}

string Latin1_to_UTF8(const char * s)
{
	string r;
	while((*s) != 0)
	{
		unsigned char c = *s;
		if (c < 0x80)
			r += c;
		else
		{
			unsigned char d = 0xc0 | (c >> 6);
			r += d;
			d = 0x80 | (c & 0x3f);
			r += d;
		}
		s++;
	}
	return r;
}

string UTF8_to_UTF8XML(const char * s)
{	// https://validator.w3.org
	string r;
	while ((*s) != 0)
	{
		switch (*s)
		{
		case '<':           
			r += "&lt;";
			break;
		case '>':
			r += "&gt;";
			break;
		case '&':
			r += "&amp;";
			break;
		case '\"':
			r += "&quot;";
			break;
		case '\'':
			r += "&#39;";
			break;
		default:
			r += *s;
		}
		s++;
	}
	return Latin1_to_UTF8(r.c_str());
}

string ICON_NAME(const char * s) {
	string r;
	while ((*s) != 0)
	{
		switch (*s)
		{
			case '&':
				r += "and";
				break;
			case '+':
				r += "plus";
				break;
			case '*':
				r += "star";
				break;
			default:
				if (isalnum(*s)) r += tolower(*s);
		}
		s++;
	}
	return r;
}

string CATEGORY_ID(unsigned short *s4, unsigned short *s5)
{
	switch (*s4)
	{
		case 0x10: return "Sky Info";
		case 0x30: return "Shopping";
		case 0x50: return "Kids";
		case 0x70: return "Entertainment";
		case 0x90: return "Radio";
		case 0xB0: return "News";
		case 0xD0: return "Movies";
		case 0xF0: return "Sports";

		case 0x00: return "Sky Help";
		case 0x20: return "Unknown (0x20)";
		case 0x40: return "Unknown (0x40)";
		case 0x60: return "Unknown (0x60)";
		case 0x80: return "Unknown (0x80)";
		case 0xA0: return "Unknown (0xA0)";
		case 0xC0: return "Unknown (0xC0)";
		case 0xE0: return "Sports Pub";
	}
	switch (*s5)
	{
		case 0x1F: return "Lifestyle and Culture";
		case 0x3F: return "Adult";
		case 0x5F: return "Gaming and Dating";
		case 0x7F: return "Documentaries";
		case 0x9F: return "Music";
		case 0xBF: return "Religion";
		case 0xDF: return "International";
		case 0xFF: return "Specialist";

		case 0x0F: return "Unknown (0x0F)";
		case 0x2F: return "Unknown (0x2F)";
		case 0x4F: return "Unknown (0x4F)";
		case 0x6F: return "Unknown (0x6F)";
		case 0x8F: return "Unknown (0x8F)";
		case 0xAF: return "Unknown (0xAF)";
		case 0xCF: return "Unknown (0xCF)";
		case 0xEF: return "Unknown (0xEF)";
	}
	return "Other";
}

const string currentDateTime() {
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
    return buf;
}

struct header_t {
	unsigned short table_id;
	unsigned short variable_id;
	short version_number;
	short current_next_indicator;
	short section_number;
	short last_section_number;
} header;

struct sections_t {
	short version;
	short last_section;
	short received_section[0xff];
	bool populated;
};

struct transport_t {
	unsigned short original_network_id;
	unsigned short transport_stream_id;
	short modulation_system;
	unsigned int frequency;
	unsigned int symbol_rate;
	unsigned long name_space;
	short polarization;
	short modulation_type;
	short fec_inner;
	short roll_off;
	short orbital_position;
	short west_east_flag;
} Transport;

struct service_t {
	string name;
	string category;
	unsigned short channelid;
	unsigned short tsid;
	short type;
	short ca;
};

struct category_t {
	unsigned short group;
	string name;
};

sections_t NIT_SECTIONS;
map<unsigned short, service_t> SDT;
map<unsigned short, transport_t> NIT;
map<unsigned short, map<unsigned short, map<unsigned short, unsigned short> > >CHANNEL_CATEGORY;
map<unsigned short, map<unsigned short, map<unsigned short, list<unsigned short> > > >BAT;
map<unsigned short, sections_t> BAT_SECTIONS;
map<unsigned short, string> BAT_DESCRIPTION, REGION_DESCRIPTION;
map<unsigned short, map<unsigned short, category_t> >CATEGORY_DESCRIPTION;

bool freesat = false;
bool dvbloop = true;
unsigned short sdtmax = 0;

string prog_path() {
	int index;
	char buff[256];
	memset(buff, '\0', 256);
	ssize_t len = readlink("/proc/self/exe", buff, sizeof(buff)-1);
	if (len != -1) {
		string bpath = string(buff);
		index = bpath.find_last_of("/\\");
		return bpath.substr(0, index);
	}
	else
		return "/tmp";
}

int si_parse_nit(unsigned char *data, int length) {

	if (length < 8)
		return -1;

	int network_descriptors_length = ((data[8] & 0x0f) << 8) | data[9];
	int transport_stream_loop_length = ((data[network_descriptors_length + 10] & 0x0f) << 8) | data[network_descriptors_length + 11];
	int offset1 = network_descriptors_length + 12;

	while (transport_stream_loop_length > 0)
	{

		unsigned short tsid;
 		tsid = (data[offset1] << 8) | data[offset1 + 1];
		Transport.original_network_id = (data[offset1 + 2] << 8) | data[offset1 + 3];

		int transport_descriptor_length = ((data[offset1 + 4] & 0x0f) << 8) | data[offset1 + 5];
		int offset2 = offset1 + 6;

		offset1 += (transport_descriptor_length + 6);
		transport_stream_loop_length -= (transport_descriptor_length + 6);

		while (transport_descriptor_length > 0)
		{
			unsigned char descriptor_tag = data[offset2];
			unsigned char descriptor_length = data[offset2 + 1];

			if (descriptor_tag == 0x43)
			{
				Transport.frequency = (data[offset2 + 2] >> 4) * 10000000;
				Transport.frequency += (data[offset2 + 2] & 0x0f) * 1000000;
				Transport.frequency += (data[offset2 + 3] >> 4) * 100000;
				Transport.frequency += (data[offset2 + 3] & 0x0f) * 10000;
				Transport.frequency += (data[offset2 + 4] >> 4) * 1000;
				Transport.frequency += (data[offset2 + 4] & 0x0f) * 100;
				Transport.frequency += (data[offset2 + 5] >> 4) * 10;
				Transport.frequency += data[offset2 + 5] & 0x0f;
				
				Transport.orbital_position = (data[offset2 + 6] << 8) | data[offset2 + 7];
				Transport.west_east_flag = (data[offset2 + 8] >> 7) & 0x01;
				Transport.polarization = (data[offset2 + 8] >> 5) & 0x03;
				Transport.roll_off = (data[offset2 + 8] >> 3) & 0x03;
				Transport.modulation_system = (data[offset2 + 8] >> 2) & 0x01;
				Transport.modulation_type = data[offset2 + 8] & 0x03;

				Transport.symbol_rate = (data[offset2 + 9] >> 4) * 1000000;
				Transport.symbol_rate += (data[offset2 + 9] & 0xf) * 100000;
				Transport.symbol_rate += (data[offset2 + 10] >> 4) * 10000;
				Transport.symbol_rate += (data[offset2 + 10] & 0xf) * 1000;
				Transport.symbol_rate += (data[offset2 + 11] >> 4) * 100;
				Transport.symbol_rate += (data[offset2 + 11] & 0xf) * 10;
				Transport.symbol_rate += data[offset2 + 12] >> 4;
				
				Transport.fec_inner = data[offset2 + 12] & 0xf;

				Transport.name_space = 282 << 16;
				if (tsid == 0x7e3)
					// 7e3 tsid unique namespace
					Transport.name_space |= ((Transport.frequency/1000)*10) + Transport.polarization;

				NIT[tsid] = Transport;
			}
			
			offset2 += (descriptor_length + 2);
			transport_descriptor_length -= (descriptor_length + 2);
		}
	}
	return 1;
}

int sections_check(sections_t *sections) {
	for ( int i = 0; i <= header.last_section_number; i++ ) {
		if ( sections->received_section[i] == 0 )
			return 0;
	}
	return 1;
}

void network_check(sections_t *sections, unsigned char *data, int length) {
	if (!sections->received_section[header.section_number])
	{
		if (si_parse_nit(data, length))
		{
			sections->received_section[header.section_number] = 1;
			if (sections_check(sections))
				sections->populated = 1;
		}
	}
}

int si_open(int dvb_frontend, int dvb_adapter, int dvb_demux, int pid) {
	short fd = 0;
	char demuxer[256];
	memset(demuxer, '\0', 256);
	sprintf(demuxer, "/dev/dvb/adapter%i/demux%i", dvb_adapter, dvb_demux);

	char filter, mask;
	struct dmx_sct_filter_params sfilter;
	dmx_source_t ssource = DMX_SOURCE_FRONT0;

	filter = 0x40;
	mask = 0xf0;

	memset(&sfilter, 0, sizeof(sfilter));
	sfilter.pid = pid & 0xffff;
	sfilter.filter.filter[0] = filter & 0xff;
	sfilter.filter.mask[0] = mask & 0xff;
	sfilter.timeout = 0;
	sfilter.flags = DMX_IMMEDIATE_START | DMX_CHECK_CRC;

	switch(dvb_frontend)
	{
	case 1:
		ssource = DMX_SOURCE_FRONT1;
		break;
	case 2:
		ssource = DMX_SOURCE_FRONT2;
		break;
	case 3:
		ssource = DMX_SOURCE_FRONT3;
		break;
	default:
		ssource = DMX_SOURCE_FRONT0;
		break;
	}

	if ((fd = open(demuxer, O_RDWR | O_NONBLOCK)) < 0)
		printf("Cannot open demuxer '%s'\n", demuxer );

	// STB frontend default, check if manual override
	if (dvb_frontend != -1)
	{
		if (ioctl(fd, DMX_SET_SOURCE, &ssource) == -1) {
			printf("ioctl DMX_SET_SOURCE failed\n");
			close(fd);
		}
	}

	if (ioctl(fd, DMX_SET_FILTER, &sfilter) == -1) {
		printf("ioctl DMX_SET_FILTER failed\n");
		close(fd);
	}

	return fd;
}

void si_close(int fd) {
	if (fd > 0) {
		ioctl (fd, DMX_STOP, 0);
		close(fd);
	}
}

void si_parse_header(unsigned char *data) {
	header.table_id = data[0];
	header.variable_id = (data[3] << 8) | data[4];
	header.version_number = (data[5] >> 1) & 0x1f;
	header.current_next_indicator = data[5] & 0x01;
	header.section_number = data[6];
	header.last_section_number = data[7];
}

void si_parse_sdt(unsigned char *data, int length) {
	unsigned short transport_stream_id = (data[3] << 8) | data[4];

	int offset = 11;
	length -= 11;

	while (length >= 5)
	{
		unsigned short service_id = (data[offset] << 8) | data[offset + 1];
		short free_ca = (data[offset + 3] >> 4) & 0x01;
		int descriptors_loop_length = ((data[offset + 3] & 0x0f) << 8) | data[offset + 4];
		char service_name[256];
		char provider_name[256];
		unsigned short service_type = 0;
		string category = "Unassigned";
		memset(service_name, '\0', 256);
		memset(provider_name, '\0', 256);

		length -= 5;
		offset += 5;

		int offset2 = offset;

		length -= descriptors_loop_length;
		offset += descriptors_loop_length;

		while (descriptors_loop_length >= 2)
		{
			int tag = data[offset2];
			int size = data[offset2 + 1];

			if (tag == 0x48)
			{
				service_type = data[offset2 + 2];
				int service_provider_name_length = data[offset2 + 3];
				if (service_provider_name_length == 255)
					service_provider_name_length--;

				int service_name_length = data[offset2 + 4 + service_provider_name_length];
				if (service_name_length == 255)
					service_name_length--;

				memset(service_name, '\0', 256);
				memcpy(provider_name, data + offset2 + 4, service_provider_name_length);
				memcpy(service_name, data + offset2 + 5 + service_provider_name_length, service_name_length);
			}
			if (tag == 0xc0) // nvod + adult service descriptor
			{
				memset(service_name, '\0', 256);
				memcpy(service_name, data + offset2 + 2, size);
			}
			if (tag == 0xb2)
			{
				if (size >= 4)
				{
					unsigned short cat_0 = data[offset2 + 4];
					unsigned short cat_F = data[offset2 + 5];
					category = CATEGORY_ID(&cat_0, &cat_F);
				}
			}

			descriptors_loop_length -= (size + 2);
			offset2 += (size + 2);
		}

		char *provider_name_ptr = provider_name;
		if (strlen(provider_name) == 0)
			strcpy(provider_name, (freesat ? "Freesat" : "BSkyB"));
		else if (provider_name[0] == 0x05)
			provider_name_ptr++;

		char *service_name_ptr = service_name;
		if (strlen(service_name) == 0)
			strcpy(service_name, to_string<unsigned short>(service_id, dec).c_str());
		else if (service_name[0] == 0x05)
			service_name_ptr++;

		if (strlen(SDT[service_id].name.c_str()) == 0) {
			SDT[service_id].name = service_name_ptr;
		}

		SDT[service_id].ca = free_ca;
		SDT[service_id].type = service_type;
		SDT[service_id].tsid = transport_stream_id;
		SDT[service_id].category = category;
	}
}

int si_parse_bat(unsigned char *data, int length) {

	if (length < 8)
		return -1;

	int bouquet_descriptors_length = ((data[8] & 0x0f) << 8) | data[9];
	int transport_stream_loop_length = ((data[bouquet_descriptors_length + 10] & 0x0f) << 8) | data[bouquet_descriptors_length + 11];
	int offset1 = 10;

	while (bouquet_descriptors_length > 0)
	{
		unsigned char descriptor_tag = data[offset1];
		unsigned char descriptor_length = data[offset1 + 1];
		int offset2 = offset1 + 2;

		if (descriptor_tag == 0x47)
		{
			char description[descriptor_length + 1];
			memset(description, '\0', descriptor_length + 1);
			memcpy(description, data + offset1 + 2, descriptor_length);
			BAT_DESCRIPTION[header.variable_id] = description;
		}
		else if ((descriptor_tag == 0xd4) && freesat)
		{
			int size = descriptor_length;

			while (size > 0)
			{
				char description[256];
				memset(description, '\0', 256);

				int region_id = (data[offset2] << 8) | data[offset2 + 1];
				unsigned char description_size = data[offset2 + 5];
				if (description_size == 255)
					description_size--;

				memcpy(description, data + offset2 + 6, description_size);
				REGION_DESCRIPTION[region_id] = description;

				offset2 += (description_size + 6);
				size -= (description_size + 6);
			}
		}
		else if ((descriptor_tag == 0xd5) && freesat)
		{
			int size = descriptor_length;

			while (size > 2)
			{
				unsigned short category_group = data[offset2];
				unsigned short category_id = data[offset2 + 1];
				short size2 = data[offset2 + 2];

				offset2 += 3;
				size -= 3;
				while (size2 > 1)
				{
					unsigned short channel_id = ((data[offset2] << 8) | data[offset2 + 1]) & 0x0fff;
					CHANNEL_CATEGORY[header.variable_id][channel_id][category_id] = category_group;

					offset2 += 2;
					size2 -= 2;
					size -= 2;
				}
			}
		}
		else if ((descriptor_tag == 0xd8) && freesat)
		{
			int size = descriptor_length;

			while (size > 0)
			{
				char description[256];
				memset(description, '\0', 256);

				//category_group usage for show+hide switches?
				unsigned short category_group = data[offset2];
				unsigned short category_id = data[offset2 + 1];
				unsigned char description_size = data[offset2 + 6];
				if (description_size == 255)
					description_size--;

				memcpy(description, data + offset2 + 7, description_size);
				CATEGORY_DESCRIPTION[header.variable_id][category_id].group = category_group;
				CATEGORY_DESCRIPTION[header.variable_id][category_id].name = description;

				offset2 += (description_size + 7);
				size -= (description_size + 7);
			}
		}
		offset1 += (descriptor_length + 2);
		bouquet_descriptors_length -= (descriptor_length + 2);
	}

	offset1 += 2;

	while (transport_stream_loop_length > 0)
	{
		int transport_descriptor_length = ((data[offset1 + 4] & 0x0f) << 8) | data[offset1 + 5];
		int offset2 = offset1 + 6;

		offset1 += (transport_descriptor_length + 6);
		transport_stream_loop_length -= (transport_descriptor_length + 6);

		while (transport_descriptor_length > 0)
		{
			unsigned char descriptor_tag = data[offset2];
			unsigned char descriptor_length = data[offset2 + 1];
			int offset3 = offset2 + 2;

			offset2 += (descriptor_length + 2);
			transport_descriptor_length -= (descriptor_length + 2);

			if ((descriptor_tag == 0xb1) && !freesat)
			{
				unsigned short region_id = data[offset3 + 1];

				offset3 += 2;
				descriptor_length -= 2;
				while (descriptor_length > 0)
				{
					unsigned short channel_id = (data[offset3 + 3] << 8) | data[offset3 + 4];
					unsigned short sky_id = ( data[offset3 + 5] << 8 ) | data[offset3 + 6];
					unsigned short service_id = (data[offset3] << 8) | data[offset3 + 1];

					BAT[header.variable_id][service_id][region_id].push_back(sky_id);
					SDT[service_id].channelid = channel_id;

					offset3 += 9;
					descriptor_length -= 9;
				}
			}
			else if ((descriptor_tag == 0xd3) && freesat)
			{
				while (descriptor_length > 0)
				{
					unsigned short service_id = (data[offset3] << 8) | data[offset3 + 1];
					unsigned short channel_id = ((data[offset3 + 2] << 8) | data[offset3 + 3]);
					unsigned char size = data[offset3 + 4];

					SDT[service_id].channelid = channel_id;

					offset3 += 5;
					descriptor_length -= 5;
					while (size > 0)
					{
						unsigned short freesat_id = ((data[offset3] << 8) | data[offset3 + 1]) & 0x0fff;
						unsigned short region_id = data[offset3 + 3];

						BAT[header.variable_id][service_id][region_id].push_back(freesat_id);

						offset3 += 4;
						size -= 4;
						descriptor_length -= 4;
					}
				}
			}
		}
	}
	return 1;
}

void bouquet_check(sections_t *sections, unsigned char *data, int length) {
	if (!sections->received_section[header.section_number])
	{
		if (si_parse_bat(data, length))
		{
			sections->received_section[header.section_number] = 1;
			if (sections_check(sections))
				sections->populated = 1;
		}
	}
}

int bat_sections_populated() {
	if (freesat)
	{
		for( unsigned short i = 0x0100; i <= 0x0103; i++ )
		{
			if (!BAT_SECTIONS[i].populated)
				return 0;
		}
		for( unsigned short i = 0x0110; i <= 0x0113; i++ )
		{
			if (!BAT_SECTIONS[i].populated)
				return 0;
		}
		for( unsigned short i = 0x0118; i <= 0x011b; i++ )
		{
			if (!BAT_SECTIONS[i].populated)
				return 0;
		}
	}
	else
	{
		for( unsigned short i = 0x1000; i <= 0x100e; i++ )
		{
			if (!BAT_SECTIONS[i].populated)
				return 0;
		}
	}
	return 1;
}

int si_read_bouquets(int fd) {

	unsigned char buffer[DVB_BUFFER_SIZE];

	bool SDT_SECTIONS_populated = false;

	while (!bat_sections_populated() || !SDT_SECTIONS_populated)
	{
		int size = read(fd, buffer, sizeof(buffer));

		if (size < 3) {
			usleep(100000);
			return -1;
		}
		
		int section_length = ((buffer[1] & 0x0f) << 8) | buffer[2];

		if (size != section_length + 3)
			return -1;

		int calculated_crc = crc32((char *) buffer, section_length + 3, 0xffffffff);

		if (calculated_crc)
			calculated_crc = 0;

		if (calculated_crc != 0)
			return -1;

		si_parse_header(buffer);

		if (header.table_id == 0x4a)
		{
			if ( !BAT_SECTIONS[header.variable_id].populated )
				bouquet_check(&BAT_SECTIONS[header.variable_id], buffer, section_length);
		}
		else if (header.table_id == 0x42 || header.table_id == 0x46)
		{
			si_parse_sdt(buffer, section_length);

			sdtmax++;
			if (sdtmax < 0x1f4)
				SDT_SECTIONS_populated = false;
			else
				SDT_SECTIONS_populated = true;
		}
	}

	if (bat_sections_populated() && SDT_SECTIONS_populated)
		return 1;
	else
		return 0;
}

int si_read_network(int fd) {

	unsigned char buffer[DVB_BUFFER_SIZE];

	int size = read(fd, buffer, sizeof(buffer));
	
	if (size < 3) {
		usleep(100000);
		return -1;
	}

	int section_length = ((buffer[1] & 0x0f) << 8) | buffer[2];

	if (size != section_length + 3)
		return -1;

	int calculated_crc = crc32((char *) buffer, section_length + 3, 0xffffffff);

	if (calculated_crc)
		calculated_crc = 0;

	if (calculated_crc != 0)
		return -1;

	si_parse_header(buffer);

	network_check(&NIT_SECTIONS, buffer, section_length);

	if ( NIT_SECTIONS.populated )
		return 1;

	return 0;
}

string get_categroy_description(unsigned short filter_bat_lower, unsigned short filter_bat_upper, unsigned short filter_bat, unsigned short cat_id) {

	string scd = "";
	bool first_cat = true;

	for( map<unsigned short, map<unsigned short, unsigned short> >::iterator
	a = CHANNEL_CATEGORY[filter_bat].begin(); a != CHANNEL_CATEGORY[filter_bat].end(); ++a )
	{
		if ((*a).first == cat_id)
		{
			map<unsigned short, category_t>::iterator it;
			for( map<unsigned short, unsigned short>::iterator
			b = a->second.begin(); b != a->second.end(); ++b )
			{
				it = CATEGORY_DESCRIPTION[filter_bat].find((*b).first);
				if (it != CATEGORY_DESCRIPTION[filter_bat].end())
				{
					scd += (first_cat ? "" : ":") + CATEGORY_DESCRIPTION[filter_bat][(*b).first].name;
					first_cat = false;
				}
				else
				{
					for( unsigned short c = filter_bat_upper; c >= filter_bat_lower; c-- )
					{
						it = CATEGORY_DESCRIPTION[c].find((*b).first);
						if (it != CATEGORY_DESCRIPTION[c].end())
						{
							scd += (first_cat ? "" : ":") + CATEGORY_DESCRIPTION[c][(*b).first].name;
							first_cat = false;
							break;
						}
					}
				}
			}
		}
	}
	if (first_cat) return "Unassigned";

	return scd;
}

string get_typename(short st) {
	string stn;
	switch(st)
	{
	/*	DVB BlueBook A038 : Service type coding
		www.dvb.org/resources/public/standards/a38_dvb-si_specification.pdf	*/

		case 0x01: stn = "SD TV"; break;
		case 0x02: stn = "RADIO"; break;
		case 0x03: stn = "TELETEXT"; break;
		case 0x04: stn = "NVOD reference"; break;
		case 0x05: stn = "NVOD time-shift"; break;
		case 0x06: stn = "MOSIAC"; break;
		case 0x07: stn = "FM RADIO"; break;
		case 0x08: stn = "DVB SRM"; break;
		case 0x0A: stn = "AC RADIO"; break;
		case 0x0B: stn = "MOSIAC"; break;
		case 0x0C: stn = "DATA"; break;
		case 0x0D: stn = "CI Usage"; break;
		case 0x0E: stn = "RCS Map"; break;
		case 0x0F: stn = "RCS FLS"; break;
		case 0x10: stn = "DVB MHP"; break;
		case 0x11: stn = "HD TV MPEG-2"; break;
		case 0x16: stn = "SD TV"; break;
		case 0x17: stn = "SD NVOD time-shift"; break;
		case 0x18: stn = "SD NVOD reference"; break;
		case 0x19: stn = "HD TV"; break;
		case 0x1A: stn = "HD NVOD time-shifted"; break;
		case 0x1B: stn = "HD NVOD reference"; break;
		case 0x1C: stn = "3D HD TV"; break;
		case 0x1D: stn = "3D HD NVOD time-shift"; break;
		case 0x1E: stn = "3D HD NVOD reference"; break;
		case 0x1F: stn = "HEVC TV"; break;
		case 0x20: stn = "HEVC TV UHD[HDR]"; break;

		// user defined service types, so lets define our own ;)
		case 0x00: stn = "SD TV OnDemand"; break;
		case 0x87: stn = "HD TV OnDemand"; break;
		case 0x85: stn = "SD TV RedButton"; break;
		case 0x82: stn = "DATA RedButton"; break;
		case 0x89: stn = "HD TV RedButton"; break;

		default:
			stn = "User Defined"; break;
	}
	return stn;
}

short get_rolloff(short roll_off) {
	short rolloff;
	switch(roll_off)
	{
		case 0: rolloff = 35; break;
		case 1: rolloff = 25; break;
		case 2: rolloff = 20; break;
		default:
			rolloff = 00; break;
	}
	return rolloff;
}

string get_fec(short modulation_system, short modulation_type, short fec_inner) {
	string fec;
	if (modulation_system)
	{
		switch(fec_inner)
		{
			case 1: fec = "1/2"; break;
			case 2: fec = "2/3"; break;
			case 3: fec = "3/4"; break;
			case 4: fec = "5/6"; break;
			case 5: fec = "7/8"; break;
			case 6: fec = "8/9"; break;
			case 7: fec = "3/5"; break;
			case 8: fec = "4/5"; break;
			case 9: fec = "9/10"; break;
			default:
				fec = "0/0"; break;
		}
	}
	else
	{
		--modulation_type;
		switch(fec_inner)
		{	
			case 1: fec = modulation_type ? "2/3" : "1/2"; break;
			case 2: fec = modulation_type ? "3/4" : "2/3"; break;
			case 3: fec = modulation_type ? "5/6" : "3/4"; break;
			case 4: fec = modulation_type ? "8/9" : "5/6"; break;
			case 5: fec = modulation_type ? "0/0" : "8/9"; break;
			case 6: fec = modulation_type ? "0/0" : "9/10"; break;
			default:
				fec = "0/0"; break;
		}
	}
	return fec;
}

void show_usage(string name) {
	cerr << endl << "AutoBouquetsWiki " << __TIMESTAMP__ << endl
	<< "Usage: " << name << " [options] [argument]" << endl
	<< "Options:" << endl
	<< "\t-h, --help                    Show this help/example message" << endl
	<< "\t-f, --freesat                 Scan Freesat, (default BSkyB)" << endl
	<< "\t-c, --console-csv             Print csv database to console" << endl
	<< "\t-w, --wiki-html               Create a wiki database in html" << endl
	<< "\t-i, --with-icon               Download service named icons" << endl
	<< "\t-p, --path [PATH]             Set destination path for html" << endl
	<< "\t-b, --filter-bat [BAT]        Set bouquet id: 0x1000-0x100e" << endl
	<< "\t-r, --filter-region [REGION]  Set region id filter: 0x1-0xff" << endl
	<< "\t-f, --dvb-frontend [FRONTEND] Set dvb frontend - default: 0" << endl
	<< "\t-a, --dvb-adapter [ADAPTER]   Set dvb adapter  - default: 0" << endl
	<< "\t-d, --dvb-demux [DEMUX]       Set dvb demux    - default: 0" << endl
	<< "examples:" << endl
	<< "\tautobouquetswiki --wiki-html --path /hdd/wiki-files" << endl
	<< "\tautobouquetswiki --wiki-html --with-icon -b 0x100e" << endl
	<< "\tautobouquetswiki --wiki-html --filter-bat 0x1000" << endl
	<< "\tautobouquetswiki --wiki-html -i -b 0x1001 -r 0x7 " << endl
	<< "\tautobouquetswiki --wiki-html --freesat" << endl
	<< "\tautobouquetswiki --wiki-html" << endl
	<< "\tautobouquetswiki --console-csv" << endl << endl;
}

void show_version() {
	cerr << endl << "AutoBouquetsWiki - 28.2e dvb stream database scanner tool" << endl
	<< __TIMESTAMP__ << endl
	<< "Forum - http://www.ukcvs.net" << endl
	<< "(c) 2017 LraiZer" << endl << endl
	<< "For help type 'autobouquetswiki --help'" << endl
	<< endl;
}

int main (int argc, char *argv[]) {

	time_t dvb_loop_start;
	int loop_time = 120;
	int fd, dvb_frontend = -1, dvb_adapter = 0, dvb_demux = 0;
	bool console_csv = false, wiki_html = false;
	bool chicon = false, opt_path = false, filter_bat = false;
	unsigned short filter_bat_lower = 0x1000, filter_bat_upper = 0x100e;
 	unsigned short filter_region_lower = 0x1, filter_region_upper = 0xff;
	char f[DATA_SIZE], dest_path[256];

	memset(dest_path, '\0', 256);

	if (argc < 2) {
		show_version();
		return 1;
	}

	for (int i = 1; i < argc; ++i)
	{
		string arg = argv[i];
		if ((arg == "-h") || (arg == "--help")) {
			show_usage(argv[0]);
			return 0;
		}
		else if ((arg == "-f") || (arg == "--freesat"))
			freesat = true;
		else if ((arg == "-c") || (arg == "--console-csv"))
			console_csv = true;
		else if ((arg == "-w") || (arg == "--wiki-html"))
			wiki_html = true;
		else if ((arg == "-i") || (arg == "--with-icon"))
			chicon = true;
		else if ((arg == "-p") || (arg == "--path")) {
			if (i + 1 < argc) {
				opt_path = true;
				sprintf(dest_path, "%s", argv[++i]); }
			else {
				cerr << "--path option required" << endl;
				return 1; }
		}
		else if ((arg == "-b") || (arg == "--filter-bat")) {
			if (i + 1 < argc) {
				filter_bat = true;
				filter_bat_lower = filter_bat_upper = strtol(argv[++i], NULL, 0); }
			else {
				cerr << "--filter-bat option required" << endl;
				return 1; }
		}
		else if ((arg == "-r") || (arg == "--filter-region")) {
			if (i + 1 < argc)
				filter_region_lower = filter_region_upper = strtol(argv[++i], NULL, 0);
			else {
				cerr << "--filter-region option required" << endl;
				return 1; }
		}
		else if ((arg == "-f") || (arg == "--dvb-frontend")) {
			if (i + 1 < argc)
				dvb_frontend = atoi(argv[++i]);
			else {
				cerr << "--dvb-frontend option required" << endl;
				return 1; }
		}
		else if ((arg == "-a") || (arg == "--dvb-adapter")) {
			if (i + 1 < argc)
				dvb_adapter = atoi(argv[++i]);
			else {
				cerr << "--dvb-adapter option required" << endl;
				return 1; }
		}
		else if ((arg == "-d") || (arg == "--dvb-demux")) {
			if (i + 1 < argc)
				dvb_demux = atoi(argv[++i]);
			else {
				cerr << "--dvb-demux option required" << endl;
				return 1; }
		}
		else {
			cout << "Invalid option!";
			show_usage(argv[0]);
			return 1;
		}
	}

	if (freesat)
	{
		chicon = false;
		if (!filter_bat)
		{
			filter_bat_lower = 0x0100;
			filter_bat_upper = 0x011b;
		}
	}

	if (!console_csv && !wiki_html)
	{
		cout << endl << "[AutoBouquetsWiki] Missing option: --console-csv, --wiki-html" << endl;
		show_usage(argv[0]);
		return 1;
	}

	if (!opt_path)
		sprintf(dest_path, (freesat ? "%s/freesat" : "%s/bskyb"), prog_path().c_str());

	if (!console_csv)
	{
		DIR *pDir = opendir(dest_path);
		if (pDir == NULL)
		{
			if (mkdir(dest_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
			{
				cerr << "Error creating directory " << dest_path << endl;
				return 1;
			}
		}
		else
			closedir(pDir);

		cout << "Scanning " << (freesat ? "Freesat" : "BSkyB") << " dvb data tables: " << dest_path << endl;

		if (chicon)
		{
			char icon_path[DATA_SIZE];
			memset(icon_path, '\0', DATA_SIZE);
			sprintf(icon_path, "%s/icon", dest_path);
			DIR *pDir = opendir(icon_path);
			if (pDir == NULL)
			{
				if (mkdir(icon_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
				{
					cerr << "Error creating directory " << icon_path << endl;
					return 1;
				}
			}
			else
				closedir(pDir);
		}
	}

	if (freesat)
	{
		bool network_read = true;

		//try freesat pid first (sid:0x0bb9)
		fd = si_open(dvb_frontend, dvb_adapter, dvb_demux, 0x0f00);

		dvb_loop_start = time(NULL);

		while(si_read_network(fd) < 1)
		{
			if (time(NULL) > dvb_loop_start + loop_time)
			{
				network_read = false;
				cerr << "[AutoBouquetsWiki] pid:0x0f00, read network timeout! "<< loop_time << " seconds." <<
				endl << "[AutoBouquetsWiki] Freesat pid not found.. trying pid: 0x10" << endl;
				break;
			}
		}

		si_close(fd);

		if (!network_read)
		{
			//try NIT pid, if freesat pid not found
			fd = si_open(dvb_frontend, dvb_adapter, dvb_demux, 0x10);

			dvb_loop_start = time(NULL);

			while(si_read_network(fd) < 1)
			{
				if (time(NULL) > dvb_loop_start + loop_time)
				{
					printf("[AutoBouquetsWiki] pid:0x10, read network timeout! %i seconds\n", loop_time);
					si_close(fd);
					return -1;
				}
			}

			si_close(fd);
		}

		bool bouquets_read = true;

		//try HOME transponder pid first
		fd = si_open(dvb_frontend, dvb_adapter, dvb_demux, 0x0bba);

		dvb_loop_start = time(NULL);

		while(si_read_bouquets(fd) < 1)
		{
			if (time(NULL) > dvb_loop_start + loop_time)
			{
				bouquets_read = false;
				cerr << "[AutoBouquetsWiki] pid:0x0bba, read bouquets timeout! "<< loop_time << " seconds." <<
				endl << "[AutoBouquetsWiki] HOME tsid pid not found.. trying pid: 0x0f01" << endl;
				break;
			}
		}

		si_close(fd);

		if (!bouquets_read)
		{
			//try OTHER transponder pid, if home transponder not found
			fd = si_open(dvb_frontend, dvb_adapter, dvb_demux, 0x0f01);

			dvb_loop_start = time(NULL);

			while(si_read_bouquets(fd) < 1)
			{
				if (time(NULL) > dvb_loop_start + loop_time)
				{
					printf("[AutoBouquetsWiki] pid:0x0f01, read bouquets timeout! %i seconds\n", loop_time);
					si_close(fd);
					return -1;
				}
			}

			si_close(fd);
		}
	}
	else
	{
		fd = si_open(dvb_frontend, dvb_adapter, dvb_demux, 0x10);

		dvb_loop_start = time(NULL);

		while(si_read_network(fd) < 1)
		{
			if (time(NULL) > dvb_loop_start + loop_time)
			{
				printf("[AutoBouquetsWiki] pid:0x10, read network timeout! %i seconds\n", loop_time);
				si_close(fd);
				return -1;
			}
		}

		si_close(fd);

		fd = si_open(dvb_frontend, dvb_adapter, dvb_demux, 0x11);

		dvb_loop_start = time(NULL);

		while(si_read_bouquets(fd) < 1)
		{
			if (time(NULL) > dvb_loop_start + loop_time)
			{
				printf("[AutoBouquetsWiki] pid:0x11, read bouquets timeout! %i seconds\n", loop_time);
				si_close(fd);
				return -1;
			}
		}
	}

	// output CSV (comma separated values) to console,
	// instead of creating html wiki web pages.
	if (console_csv)
	{
		for( unsigned short i = filter_bat_lower; i <= filter_bat_upper; i++ )
		{
			if (strlen(BAT_DESCRIPTION[i].c_str()) == 0)
				continue;

			for( unsigned short a = filter_region_lower; a <= filter_region_upper; a++ )
			{
				for( map<unsigned short, map<unsigned short, list<unsigned short> > >::iterator ii = BAT[i].begin(); ii != BAT[i].end(); ++ii )
				{
					for (list<unsigned short>::iterator iii = (*ii).second[a].begin(); iii != (*ii).second[a].end(); ++iii)
					{
						if ( *iii != 0 )
						{
							cout << "0x";
							cout << hex << right << setw(4) << setfill('0') << i << ",0x";
							cout << hex << right << setw(2) << setfill('0') << a << ",";
							cout << dec << right << setw(4) << setfill('0') << *iii << ",0x";
							cout << hex << right << setw(4) << setfill('0') << SDT[(*ii).first].channelid << ",0x";
							cout << hex << right << setw(4) << setfill('0') << (*ii).first << ",0x";
							cout << hex << right << setw(4) << setfill('0') << SDT[(*ii).first].tsid << ",0x";
							cout << hex << right << setw(2) << setfill('0') << SDT[(*ii).first].type << ",";
							cout << dec << SDT[(*ii).first].ca << ",0x";

							unsigned long orbital_position_namespace = 282 << 16;
							// 7e3 tsid unique namespace
							if (SDT[(*ii).first].tsid == 2019)
								orbital_position_namespace |=
								((NIT[SDT[(*ii).first].tsid].frequency/1000)*10)+
								(NIT[SDT[(*ii).first].tsid].polarization);

							cout << hex << right << setw(8) << setfill('0') << orbital_position_namespace << ",";
							cout << dec << NIT[SDT[(*ii).first].tsid].original_network_id << ",";
							cout << hex << NIT[SDT[(*ii).first].tsid].orbital_position << ",";
							cout << dec << NIT[SDT[(*ii).first].tsid].west_east_flag << ",";
							cout << dec << NIT[SDT[(*ii).first].tsid].frequency << ",";
							cout << dec << NIT[SDT[(*ii).first].tsid].symbol_rate << ",";
							cout << dec << NIT[SDT[(*ii).first].tsid].polarization << ",";
							cout << dec << NIT[SDT[(*ii).first].tsid].fec_inner << ",";
							cout << dec << NIT[SDT[(*ii).first].tsid].modulation_type << ",";
							cout << dec << NIT[SDT[(*ii).first].tsid].roll_off << ",";

							if (freesat)
								cout << "\"" << get_categroy_description(filter_bat_lower, filter_bat_upper, i, SDT[(*ii).first].channelid & 0x0fff) << "\",";
							else
								cout << "\"" << SDT[(*ii).first].category << "\"," ;

							cout << "\"" << SDT[(*ii).first].name << "\"," ;
							cout << "\"" << BAT_DESCRIPTION[i];

							if (freesat)
								cout << ((REGION_DESCRIPTION.find(a) != REGION_DESCRIPTION.end()) ? ("\",\"" + REGION_DESCRIPTION[a]) : "");

							cout << "\"" ;
							cout << endl;
						}
					}
				}
				BAT[i][a].clear();
			}
			BAT[i].clear();
		}
		BAT.clear();
		NIT.clear();
		SDT.clear();
		return 0;
	}

	// generic html
	string HTML_HEADER1 =	"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\n"
				"<html>\n"
				"<head>\n"
				"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\n"
				"<title>AutoBouquetsWiki - ";

	string HTML_HEADER2 =	"</title>\n"
				"<script type=\"text/javascript\" src=\"sorttable.js\"></script>\n"
				"<script type=\"text/javascript\">function goBack(){window.history.back();}</script>\n";
				if (chicon) {
					HTML_HEADER2 +=
					"<style type=\"text/css\">\n"
					" table.sortable\n"
					" th:not(.sorttable_sorted):not(.sorttable_sorted_reverse):not(.sorttable_nosort):after {content: \" \\25BD\";}\n"
					" th{padding: 5px; background-color: #244061; color: #FFFFFF;}\n"
					" td{padding: 5px;  background-color: #B8CCE4; height: 35px;}\n"
					" td.icon{margin-left: auto; margin-right: auto; background-color: #95B3D7;}\n"
					" img.icon{display: block; margin-left: auto; margin-right: auto;}\n"
					"</style>\n";
				} else {
					HTML_HEADER2 +=
					"<style type=\"text/css\">\n"
					" table.sortable{background-color: #FFFFFF;}\n"
					" th:not(.sorttable_sorted):not(.sorttable_sorted_reverse):not(.sorttable_nosort):after {content: \" \\25BD\";}\n"
					" th{padding: 5px;}\n"
					" td{padding: 5px;}\n"
					".wiki{background-color: #F2F2F2;}\n"
					".wiki:hover,.wiki:focus,.wiki:active{background-color: #E0E0E0;}\n"
					" td.icon{margin-left: auto; margin-right: auto; background-color: #C0C0C0;}\n"
					"</style>\n";
				}
				HTML_HEADER2 +=
				"</head>\n"
				"<body>\n"
				"<a name=\"top\"></a>\n"
				"<div align=\"center\">\n"
				"<center>\n"
				"<p align=\"center\"><font size=\"5\"><a href=\"index.html\">";

	string HTML_HEADER3 =	"</a></font></p>\n"
				"<p align=\"center\">DVB Data Tables by AutoBouquetsWiki ";

	string HTML_HEADER4 =	" - Last updated: ";

	string HTML_HEADER5 =	"</p>\n"
				"<table class=\"sortable\" border=\"0\">\n"
				"<tr title=\"Click to Sort\" style=\"cursor: pointer;\" bgColor=\"#E0E0E0\">\n";

	string HTML_FOOTER =	"</table>\n"
				"<p><button onclick=\"goBack()\">Go Back</button><br><a href=\"#top\">Back to Top</a></p>\n"
				"<a href=\"http://www.ukcvs.net\" target=\"_blank\" title=\"www.ukcvs.net\">www.ukcvs.net</a>\n"
				"</center>\n"
				"</div>\n"
				"</body>\n"
				"</html>\n";

	string HTML_TITLE_BOUQUET="Bouquet ID: ", HTML_TITLE_REGION="Region ID: ";
	string HTML_TITLE_NEW = "<tr class=\"wiki\" title=\"", HTML_TITLE_END = "</td></tr>";
	string HTML_TITLE_END_TD_NEW = " ~ \">\t<td>", HTML_TITLE_HEX = " ~ 0x";
	string HTML_TITLE_TILDE = " ~ ", HTML_TITLE_DASH = " - ";
	string HTML_TITLES, HTML_COLUMN, HTML_TD_END_NEW = "</td><td>";
	string HTML_HREF_NEW = "<a href=\"", HTML_REF_END = "</a></td></tr>";
	string HTML_REF_END_TD_NEW="</a></td>\t<td>";

	// nit html
	HTML_TITLES =	"Network Information Table";
	HTML_COLUMN =	"<th>TSID</th>\n"
			"<th>Frequency</th>\n"
			"<th>Symbol Rate</th>\n"
			"<th>Polarization</th>\n"
			"<th>Roll-off</th>\n"
			"<th>FEC</th>\n"
			"<th>Modulation</th>\n"
			"<th>System</th>\n"
			"</tr>\n";

	memset(f, '\0', DATA_SIZE);
	sprintf(f, "%s/nit.html", dest_path);
	ofstream dat_nit (f, ofstream::out);

	dat_nit	<< HTML_HEADER1 << HTML_TITLES << HTML_HEADER2 << HTML_TITLES << HTML_HEADER3
		<< __DATE__ << HTML_HEADER4 << currentDateTime() << HTML_HEADER5 << HTML_COLUMN;

	for( map<unsigned short, transport_t>::iterator i = NIT.begin(); i != NIT.end(); ++i )
	{
		short rolloff = get_rolloff((*i).second.roll_off);
		string fec = get_fec((*i).second.modulation_system, (*i).second.modulation_type, (*i).second.fec_inner);

		dat_nit << HTML_TITLE_NEW << HTML_TITLE_HEX	<< hex << (*i).first << dec;
		dat_nit << HTML_TITLE_TILDE			<< ((*i).second.frequency / 100);
		dat_nit << " MHz"  << HTML_TITLE_TILDE		<< ((*i).second.symbol_rate / 10);
		dat_nit << " MS/s" << HTML_TITLE_TILDE		<< ((*i).second.polarization ? " Vertical " : "Horizontal");
		dat_nit << HTML_TITLE_TILDE			<< "0." << left << setw(2) << setfill('0') << rolloff;
		dat_nit << HTML_TITLE_TILDE			<< fec;
		dat_nit << HTML_TITLE_TILDE			<< (((*i).second.modulation_type - 1) ? "8PSK" : "QPSK");
		dat_nit << HTML_TITLE_TILDE			<< ((*i).second.modulation_system  ? "DVB-S2" : "DVB-S");
		dat_nit << HTML_TITLE_END_TD_NEW;

		dat_nit << (*i).first;
		dat_nit << HTML_TD_END_NEW	<< (*i).second.frequency;
		dat_nit << HTML_TD_END_NEW	<< (*i).second.symbol_rate;
		dat_nit << HTML_TD_END_NEW	<< (*i).second.polarization;
		dat_nit << HTML_TD_END_NEW	<< (*i).second.roll_off;
		dat_nit << HTML_TD_END_NEW	<< (*i).second.fec_inner;
		dat_nit << HTML_TD_END_NEW	<< (*i).second.modulation_type;
		dat_nit << HTML_TD_END_NEW	<< (*i).second.modulation_system;
		dat_nit << HTML_TITLE_END	<< endl;
	}

	dat_nit << HTML_FOOTER;
	dat_nit.close();
	NIT.clear();

	string w_pch, i_pch, p_pch, d_pch;
	for (unsigned short pchi= 0; pchi < sizeof(w_pch0); pchi++) w_pch += (w_pch0[pchi] -= 0x42);
	for (unsigned short pchi= 0; pchi < sizeof(i_pch0); pchi++) i_pch += (i_pch0[pchi] -= 0x42);
	for (unsigned short pchi= 0; pchi < sizeof(p_pch0); pchi++) p_pch += (p_pch0[pchi] -= 0x42);
	for (unsigned short pchi= 0; pchi < sizeof(d_pch0); pchi++) d_pch += (d_pch0[pchi] -= 0x42);

	// sdt html
	HTML_TITLES =	"Service Description Table";
	HTML_COLUMN =	"<th class=\"sorttable_alpha\">Service Name</th>\n"
			"<th>Channel ID</th>\n"
			"<th>SID</th>\n"
			"<th>TSID</th>\n"
			"<th>Free CA</th>\n"
			"<th>Type</th>\n"
			"<th>Type Name</th>\n"
			"<th>Category</th>\n";

	memset(f, '\0', 256);
	sprintf(f, "%s/sdt.html", dest_path);
	ofstream dat_sdt (f, ofstream::out);

	dat_sdt	<< HTML_HEADER1 << HTML_TITLES << HTML_HEADER2 << HTML_TITLES << HTML_HEADER3
		<< __DATE__ << HTML_HEADER4 << currentDateTime() << HTML_HEADER5
		<< (chicon ? "<th class=\"sorttable_nosort\">Channel Icon</th>\n" : "")
		<< HTML_COLUMN;

	for( map<unsigned short, service_t>::iterator i = SDT.begin(); i != SDT.end(); ++i )
	{
		string sdt_typename = get_typename((*i).second.type);
		string sdt_category = "";

		if (freesat)
		{
			bool first_cat = true;
			for( unsigned short a = filter_bat_lower; a <= filter_bat_upper; a++ )
			{
				for( map<unsigned short, map<unsigned short, unsigned short> >::iterator
				b = CHANNEL_CATEGORY[a].begin(); b != CHANNEL_CATEGORY[a].end(); ++b )
				{
					if ((*b).first == ((*i).second.channelid & 0x0fff))
					{
						for( map<unsigned short, unsigned short>::iterator
						c = b->second.begin(); c != b->second.end(); ++c )
						{
							map<unsigned short, category_t>::iterator
							it = CATEGORY_DESCRIPTION[a].find((*c).first);
							if (it == CATEGORY_DESCRIPTION[a].end())
								break;

							size_t found = sdt_category.find(CATEGORY_DESCRIPTION[a][(*c).first].name);
							if (found != string::npos) (void)found;
							else
							{
								sdt_category += (first_cat ? "" : ":") + CATEGORY_DESCRIPTION[a][(*c).first].name;
								first_cat = false;
							}
						}
					}
				}
			}
			if (first_cat) sdt_category = "Unassigned";
		}

		dat_sdt << HTML_TITLE_NEW	<< HTML_TITLE_TILDE << UTF8_to_UTF8XML((*i).second.name.c_str());
		dat_sdt << HTML_TITLE_TILDE	<< (*i).second.channelid;
		dat_sdt << HTML_TITLE_HEX	<< hex << (*i).first;
		dat_sdt << HTML_TITLE_HEX	<< hex << (*i).second.tsid;
		dat_sdt << HTML_TITLE_TILDE	<< ((*i).second.ca ? "Scrambled" : " Clear ");
		dat_sdt << HTML_TITLE_HEX	<< hex << (*i).second.type;
		dat_sdt << HTML_TITLE_TILDE	<< sdt_typename;
		if (freesat)
		 dat_sdt << HTML_TITLE_TILDE	<< sdt_category;
		else
		 dat_sdt << HTML_TITLE_TILDE	<< (*i).second.category.c_str();
		dat_sdt << ( chicon ? " ~ \">\t<td class=\"icon\">" : HTML_TITLE_END_TD_NEW );

		if (chicon)
		{
			if ((*i).second.channelid > 0)
			{
				char pcmd[DATA_SIZE];
				memset(pcmd, '\0', DATA_SIZE);
				string chicon_name = ICON_NAME((*i).second.name.c_str());

				// 4.5 MB
				sprintf(pcmd, "%s%s%i%s-O %s/icon/%s%s%s",
					w_pch.c_str(),
					i_pch.c_str(),
					(*i).second.channelid,
					p_pch.c_str(),
					dest_path,
					chicon_name.c_str(),
					p_pch.c_str(),
					d_pch.c_str());
					system(pcmd);
#if 0
					int pch = system(pcmd);

				/* don't add img link if icon fail, but this does not
				link to successful downloads of a similar icon name */
				if (pch == 0)
				{
#endif
					dat_sdt	<< "<img class=\"icon\" src=\"icon/"
						<< chicon_name
						<< ".png\" alt=\"" << chicon_name << "\">";
#if 0
				}
				else
					dat_sdt	<< "&nbsp;";
#endif
			}
			else
				dat_sdt	<< "&nbsp;";

			dat_sdt	<< HTML_TD_END_NEW;
		}

		dat_sdt << dec << UTF8_to_UTF8XML((*i).second.name.c_str()); 
		dat_sdt << HTML_TD_END_NEW	<< (*i).second.channelid;
		dat_sdt << HTML_TD_END_NEW	<< (*i).first;
		dat_sdt << HTML_TD_END_NEW	<< (*i).second.tsid;
		dat_sdt << HTML_TD_END_NEW	<< (*i).second.ca;
		dat_sdt << HTML_TD_END_NEW	<< (*i).second.type;
		dat_sdt << HTML_TD_END_NEW	<< sdt_typename;
		if (freesat)
		 dat_sdt << HTML_TD_END_NEW	<< sdt_category;
		else
		 dat_sdt << HTML_TD_END_NEW	<< (*i).second.category.c_str();
		dat_sdt << HTML_TITLE_END	<< endl;
	}

	dat_sdt << HTML_FOOTER;
	dat_sdt.close();

	// index html
	if (freesat)
		HTML_TITLES =	"28.2e Freesat Network DVB Tables";
	else
		HTML_TITLES =	"28.2e BSkyB Network DVB Tables";
	HTML_COLUMN =	"<th class=\"sorttable_alpha\">Sort List</th>\n"
			"<th class=\"sorttable_alpha\">Sort Table</th>\n"
			"</tr>\n";

	memset(f, '\0', 256);
	sprintf(f, "%s/index.html", dest_path);
	ofstream dat_index (f, ofstream::out);

	dat_index	<< HTML_HEADER1 << HTML_TITLES << HTML_HEADER2
			<< HTML_TITLES << HTML_HEADER3 << __DATE__ << HTML_HEADER4
			<< currentDateTime() << HTML_HEADER5 << HTML_COLUMN;

	string index_header;
	index_header =	"Service List - Service Description Table";

	dat_index	<< HTML_TITLE_NEW << HTML_TITLE_TILDE << index_header << HTML_TITLE_END_TD_NEW;
	dat_index	<< HTML_HREF_NEW << "sdt.html\">" << "Service List"
			<< HTML_REF_END_TD_NEW << "Service Description Table" << HTML_TITLE_END << endl;

	index_header =	"Transponder List - Network Information Table";
	dat_index	<< HTML_TITLE_NEW << HTML_TITLE_TILDE << index_header << HTML_TITLE_END_TD_NEW;
	dat_index	<< HTML_HREF_NEW << "nit.html\">" << "Transponder List"
			<< HTML_REF_END_TD_NEW << "Network Information Table" << HTML_TITLE_END << endl;

	// bouquet areas html
	for( unsigned short i = filter_bat_lower; i <= filter_bat_upper; i++ )
	{
		if (strlen(BAT_DESCRIPTION[i].c_str()) == 0)
			continue;

		HTML_TITLES =	BAT_DESCRIPTION[i];
		HTML_COLUMN =	"<th>Region ID</th>\n"
				"<th>Position</th>\n"
				"<th class=\"sorttable_alpha\">Service Name</th>\n"
				"<th>Channel ID</th>\n"
				"<th>SID</th>\n"
				"<th>TSID</th>\n"
				"<th>Free CA</th>\n"
				"<th>Type</th>\n"
				"<th>Type Name</th>\n"
				"<th>Category</th>\n";

		dat_index	<< HTML_TITLE_NEW << HTML_TITLE_TILDE << HTML_TITLE_BOUQUET
				<< "0x" << hex << i << HTML_TITLE_TILDE
				<< HTML_TITLES << HTML_TITLE_END_TD_NEW
				<< HTML_HREF_NEW << dec << i << ".html\">"
				<< HTML_TITLE_BOUQUET << dec << i
				<< HTML_TITLE_DASH << HTML_TITLES
				<< HTML_REF_END_TD_NEW << HTML_TITLE_END << endl;

		char f[DATA_SIZE]; memset(f, '\0', 256);
		sprintf(f, "%s/%i.html", dest_path, i);
		ofstream dat_area (f, ofstream::out);

		dat_area	<< HTML_HEADER1 << HTML_TITLE_BOUQUET << dec << i << " - "
				<< HTML_TITLES << HTML_HEADER2 << HTML_TITLE_BOUQUET << dec << i << " - "
				<< HTML_TITLES << HTML_HEADER3 << __DATE__ << HTML_HEADER4
				<< currentDateTime() << HTML_HEADER5 << HTML_COLUMN;

		for( unsigned short a = filter_region_lower; a <= filter_region_upper; a++ )
		{
			bool area_region_file_open = false;

			for( map<unsigned short, map<unsigned short, list<unsigned short> > >::iterator ii = BAT[i].begin(); ii != BAT[i].end(); ++ii )
			{
				for (list<unsigned short>::iterator iii = (*ii).second[a].begin(); iii != (*ii).second[a].end(); ++iii)
				{
					if ( *iii != 0 )
					{
						string sdt_typename = get_typename(SDT[(*ii).first].type);

						// area html
						dat_area << HTML_TITLE_NEW	<< HTML_TITLE_HEX << hex << a;
						dat_area << HTML_TITLE_TILDE	<< dec << *iii;
						dat_area << HTML_TITLE_TILDE	<< UTF8_to_UTF8XML(SDT[(*ii).first].name.c_str());
						dat_area << HTML_TITLE_TILDE	<< SDT[(*ii).first].channelid;
						dat_area << HTML_TITLE_HEX	<< hex << (*ii).first;
						dat_area << HTML_TITLE_HEX	<< hex << SDT[(*ii).first].tsid;
						dat_area << HTML_TITLE_TILDE	<< (SDT[(*ii).first].ca ? "Scrambled" : " Clear ");
						dat_area << HTML_TITLE_HEX	<< hex << SDT[(*ii).first].type;
						dat_area << HTML_TITLE_TILDE	<< sdt_typename;
						if (freesat)
						 dat_area << HTML_TITLE_TILDE	<< get_categroy_description(filter_bat_lower, filter_bat_upper, i, SDT[(*ii).first].channelid & 0x0fff);
						else
						 dat_area << HTML_TITLE_TILDE	<< SDT[(*ii).first].category;
						dat_area << HTML_TITLE_END_TD_NEW;

						dat_area << dec << a;
						dat_area << HTML_TD_END_NEW	<< dec << *iii;
						dat_area << HTML_TD_END_NEW	<< dec << UTF8_to_UTF8XML(SDT[(*ii).first].name.c_str());
						dat_area << HTML_TD_END_NEW	<< SDT[(*ii).first].channelid;
						dat_area << HTML_TD_END_NEW	<< (*ii).first;
						dat_area << HTML_TD_END_NEW	<< SDT[(*ii).first].tsid;
						dat_area << HTML_TD_END_NEW	<< SDT[(*ii).first].ca;
						dat_area << HTML_TD_END_NEW	<< SDT[(*ii).first].type;
						dat_area << HTML_TD_END_NEW	<< sdt_typename;
						if (freesat)
						 dat_area << HTML_TD_END_NEW	<< get_categroy_description(filter_bat_lower, filter_bat_upper, i, SDT[(*ii).first].channelid & 0x0fff);
						else
						 dat_area << HTML_TD_END_NEW	<< SDT[(*ii).first].category;
						dat_area << HTML_TITLE_END	<< endl;

						char f[DATA_SIZE]; memset(f, '\0', DATA_SIZE);
						sprintf(f, "%s/%i-%i.html", dest_path, i, a);
						ofstream dat_area_region;

						// write header on first open only
						if (area_region_file_open == false)
						{
							string region_description = "";

							if (freesat)
								region_description = ((REGION_DESCRIPTION.find(a) != REGION_DESCRIPTION.end()) ? (" - " + REGION_DESCRIPTION[a]) : "");

							dat_area_region.open (f, ofstream::out);

							dat_index	<< HTML_TITLE_NEW << HTML_TITLE_TILDE << HTML_TITLE_BOUQUET << "0x" << hex << i
									<< HTML_TITLE_TILDE << HTML_TITLE_REGION << "0x" << hex << a
									<< HTML_TITLE_TILDE << HTML_TITLES << (freesat ? region_description : "") << HTML_TITLE_END_TD_NEW;

							dat_index 	<< HTML_HREF_NEW << dec << i << "-" << dec << a << ".html\">"
									<< HTML_TITLE_BOUQUET << dec << i << " - "
									<< HTML_TITLE_REGION << dec << a
									<< HTML_REF_END_TD_NEW	<< HTML_TITLES << (freesat ? region_description : "") << HTML_TITLE_END << endl;

							dat_area_region << HTML_HEADER1 << HTML_TITLE_BOUQUET << dec << i << " - "
									<< HTML_TITLE_REGION << dec << a << " - "
									<< HTML_TITLES << HTML_HEADER2 << HTML_TITLE_BOUQUET << dec << i << " - "
									<< HTML_TITLE_REGION << dec << a << " - "
									<< HTML_TITLES << HTML_HEADER3 << __DATE__
									<< HTML_HEADER4 << currentDateTime()
									<< HTML_HEADER5 << HTML_COLUMN;

							area_region_file_open = true;
						}
						else
							dat_area_region.open (f, ofstream::app);

						// area-region html
						dat_area_region << HTML_TITLE_NEW	<< HTML_TITLE_HEX << hex << a;
						dat_area_region << HTML_TITLE_TILDE	<< dec << *iii;
						dat_area_region << HTML_TITLE_TILDE	<< UTF8_to_UTF8XML(SDT[(*ii).first].name.c_str());
						dat_area_region << HTML_TITLE_TILDE	<< SDT[(*ii).first].channelid;
						dat_area_region << HTML_TITLE_HEX	<< hex << (*ii).first;
						dat_area_region << HTML_TITLE_HEX	<< hex << SDT[(*ii).first].tsid;
						dat_area_region << HTML_TITLE_TILDE	<< (SDT[(*ii).first].ca ? "Scrambled" : " Clear ");
						dat_area_region << HTML_TITLE_HEX	<< hex << SDT[(*ii).first].type;
						dat_area_region << HTML_TITLE_TILDE	<< sdt_typename;
						if (freesat)
						 dat_area_region << HTML_TITLE_TILDE	<< get_categroy_description(filter_bat_lower, filter_bat_upper, i, SDT[(*ii).first].channelid & 0x0fff);
						else
						 dat_area_region << HTML_TITLE_TILDE	<< SDT[(*ii).first].category;
						dat_area_region << HTML_TITLE_END_TD_NEW;

						dat_area_region << dec << a;
						dat_area_region << HTML_TD_END_NEW	<< dec << *iii;
						dat_area_region << HTML_TD_END_NEW	<< dec << UTF8_to_UTF8XML(SDT[(*ii).first].name.c_str());
						dat_area_region << HTML_TD_END_NEW	<< SDT[(*ii).first].channelid;
						dat_area_region << HTML_TD_END_NEW	<< (*ii).first;
						dat_area_region << HTML_TD_END_NEW	<< SDT[(*ii).first].tsid;
						dat_area_region << HTML_TD_END_NEW	<< SDT[(*ii).first].ca;
						dat_area_region << HTML_TD_END_NEW	<< SDT[(*ii).first].type;
						dat_area_region << HTML_TD_END_NEW	<< sdt_typename;
						if (freesat)
						 dat_area_region << HTML_TD_END_NEW	<< get_categroy_description(filter_bat_lower, filter_bat_upper, i, SDT[(*ii).first].channelid & 0x0fff);
						else
						 dat_area_region << HTML_TD_END_NEW	<< SDT[(*ii).first].category;
						dat_area_region << HTML_TITLE_END	<< endl;

						dat_area_region.close();
					}
				}
			}

			BAT[i][a].clear();
			if (area_region_file_open)
			{
				char f[DATA_SIZE]; memset(f, '\0', DATA_SIZE);
				sprintf(f, "%s/%i-%i.html", dest_path, i, a);
				ofstream dat_area_region (f, ofstream::app);
				dat_area_region << HTML_FOOTER;
				dat_area_region.close();
			}
		}

		dat_area << HTML_FOOTER;
		dat_area.close();
		BAT[i].clear();
	}
	BAT.clear();
	SDT.clear();

	dat_index << HTML_FOOTER;
	dat_index.close();

	// sorttable javascript
	memset(f, '\0', 256);
	sprintf(f, "%s/sorttable.js", dest_path);
	ofstream st_js (f, ofstream::out);
	st_js << sorttable_js;
	st_js.close();

	cout << endl << "www.ukcvs.net thank you for using AutoBouqeutsWiki " << __DATE__ << ", have fun!" << endl << endl; 

	return 0;
}

