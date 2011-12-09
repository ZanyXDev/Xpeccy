#include "filetypes.h"

int loadTRD(Floppy* flp, const char* name) {
	std::ifstream file(name,std::ios::binary);
	if (!file.good()) return ERR_CANT_OPEN;
	file.seekg(0,std::ios::end);
	size_t len = file.tellg();
	if (((len & 0xfff) != 0) || (len == 0) || (len > 0xa8000)) return ERR_TRD_LEN;
	file.seekg(0x8e7,std::ios::beg);
	if (file.peek() != 0x10) return ERR_TRD_SIGN;
	file.seekg(0);
	flpFormat(flp);
	int i=0;
	uint8_t* trackBuf = new uint8_t[0x1000];
	do {
		file.read((char*)trackBuf,0x1000);
		flpFormTRDTrack(flp,i,trackBuf);
		i++;
	} while  (!file.eof());
	delete(trackBuf);
	flp->path = std::string(name);
	flpSetFlag(flp,FLP_INSERT,true);
	flpSetFlag(flp,FLP_CHANGED,false);
	return ERR_OK;
}

int saveTRD(Floppy* flp, const char* name) {
	uint8_t* img = new uint8_t[0xa0000];
	uint8_t* dptr = img;
	for (int i = 0; i < 160; i++) {
		for (int j = 1; j < 17; j++) {
			if (!flp->getSectorData(i,j,dptr,256)) {
				delete(img);
				return ERR_TRD_SNF;
			}
			dptr+=256;
		}
	}
	std::ofstream file(name,std::ios::binary);
	if (!file.good()) {
		delete(img);
		return ERR_CANT_OPEN;
	}
	file.write((char*)img,0xa0000);
	file.close();
	flpSetFlag(flp,FLP_CHANGED,false);
	delete(img);
	return ERR_OK;
}
