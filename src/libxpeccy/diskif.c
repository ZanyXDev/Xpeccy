#include <stdio.h>
#include <string.h>

#include "fdc.h"

struct DiskHW {
	const int id;
	void(*reset)(struct DiskIF*);
	int(*in)(struct DiskIF*,int,int*,int);
	int(*out)(struct DiskIF*,int,int,int);
	void(*sync)(struct DiskIF*,int);
};

int fdcFlag = 0;

// dummy (none)

void dumReset(DiskIF* dif) {}
int dumIn(DiskIF* dif, int port, int* res,int dos) {return 0;}
int dumOut(DiskIF* dif, int port, int val,int dos) {return 0;}
void dumSync(DiskIF* dif, int ns) {}

// overall

void fdcSync(FDC* fdc, int ns) {
	if (fdc->plan == NULL) return;	// fdc does nothing
	fdc->wait -= ns;
	fdc->tns += ns;
	while ((fdc->wait < 0) && (fdc->plan != NULL)) {
		if (fdc->plan[fdc->pos] != NULL) {
			fdc->plan[fdc->pos](fdc);
		} else {
			fdc->plan = NULL;
		}
	}
}

void dhwSync(DiskIF* dif, int ns) {
	fdcSync(dif->fdc, ns);
}

// BDI (VG93)

void vgReset(FDC*);
unsigned char vgRead(FDC*, int);
void vgWrite(FDC*, int, unsigned char);
void vgSetMR(FDC*, int);

int bdiGetPort(int port) {
	int res = 0;
	if ((port & 0x9f) == 0x9f) {			// 1xxxxx11 : bdi system port
		res = BDI_SYS;
	} else {
		switch (port & 0xff) {			// 0xxxxx11 : vg93 registers
			case 0x1f: res = FDC_COM; break;	// 000xxx11
			case 0x3f: res = FDC_TRK; break;	// 001xxx11
			case 0x5f: res = FDC_SEC; break;	// 010xxx11
			case 0x7f: res = FDC_DATA; break;	// 011xxx11
		}
	}
	return res;
}

int bdiIn(DiskIF* dif, int port, int* res, int dos) {
	if (!dos) return 0;
	port = bdiGetPort(port);
//	printf("in BDI port %.2X\n",port);
	if (port == 0) {
		return 0;
	} else if (port == BDI_SYS) {
		*res = (dif->fdc->irq ? 0x80 : 0x00) | (dif->fdc->drq ? 0x40 : 0x00);
	} else {
		*res = vgRead(dif->fdc, port);
	}
	return 1;
}

int bdiOut(DiskIF* dif, int port, int val, int dos) {
	if (!dos) return 0;
	port = bdiGetPort(port);
//	printf("out BDI port %.2X,%.2X\n",port,val);
	if (port == 0) {
		return 0;
	} else if (port == BDI_SYS) {
		dif->fdc->flp = dif->fdc->flop[val & 3];	// select floppy
		vgSetMR(dif->fdc,(val & 0x04) ? 1 : 0);		// master reset
		dif->fdc->block = (val & 0x08) ? 1 : 0;
		dif->fdc->side = (val & 0x10) ? 0 : 1;		// side
		dif->fdc->mfm = (val & 0x40) ? 1 : 0;
	} else {
		vgWrite(dif->fdc, port, val);
	}
	return 1;
}

void bdiReset(DiskIF* dif) {
	vgReset(dif->fdc);
	bdiOut(dif, 0xff, 0x00, 1);
}

void bdiSync(DiskIF* dif, int ns) {
	fdcSync(dif->fdc, ns);
}

// +3DOS (uPD765)

void uReset(FDC*);
unsigned char uRead(FDC*, int);
void uWrite(FDC*, int, unsigned char);

int pdosGetPort(int p) {
	int port = -1;
	if ((p & 0xf002) == 0x2000) port = 0;		// A0 input of upd765
	if ((p & 0xf002) == 0x3000) port = 1;		// 0:status(r), 1:data(rw)
	return port;
}

int pdosIn(DiskIF* dif, int port, int* res, int dos) {
	port = pdosGetPort(port);
	if (port < 0) return 0;
//	printf("in %.4X\n",port);
	*res = uRead(dif->fdc, port);
	return 1;
}

int pdosOut(DiskIF* dif, int port, int val, int dos) {
	port = pdosGetPort(port);
	if (port < 0) return 0;
	uWrite(dif->fdc, port, val);
	return 1;
}

void pdosReset(DiskIF* dif) {
	uReset(dif->fdc);
}

// bk

void vp1_reset(FDC*);
unsigned short vp1_rd(FDC*, int);
void vp1_wr(FDC*, int, unsigned short);

void bkdReset(DiskIF* dif) {
	vp1_reset(dif->fdc);
}

// rd doesn't using *res as result, it will return full 16-bit value
int bkdIn(DiskIF* dif, int port, int* res, int dos) {
	return vp1_rd(dif->fdc, port & 1) & 0xffff;
}

// wr will use *dos* argument as 16-bit value to write
int bkdOut(DiskIF* dif, int port, int val, int dos) {
	vp1_wr(dif->fdc, port & 1, dos & 0xffff);
	return 1;
}

// common

static DiskHW dhwTab[] = {
	{DIF_NONE,&dumReset,&dumIn,&dumOut,&dumSync},
	{DIF_BDI,&bdiReset,&bdiIn,&bdiOut,&dhwSync},
	{DIF_P3DOS,&pdosReset,&pdosIn,&pdosOut,&dhwSync},
	{DIF_SMK512,&bkdReset,&bkdIn,&bkdOut,&dhwSync},
	{DIF_END,NULL,NULL,NULL,NULL}
};

DiskHW* findDHW(int id) {
	DiskHW* dif = NULL;
	int idx = 0;
	while (dhwTab[idx].id != DIF_END) {
		if (dhwTab[idx].id == id) {
			dif = &dhwTab[idx];
			break;
		}
		idx++;
	}
	return dif;
}

void difSetHW(DiskIF* dif, int type) {
	dif->hw = findDHW(type);
	if (!dif->hw)
		dif->hw = findDHW(DIF_NONE);
	dif->type = dif->hw->id;
}

DiskIF* difCreate(int type) {
	DiskIF* dif = (DiskIF*)malloc(sizeof(DiskIF));
	dif->fdc = (FDC*)malloc(sizeof(FDC));
	memset(dif->fdc,0x00,sizeof(FDC));
	dif->fdc->wait = -1;
	dif->fdc->plan = NULL;
	dif->fdc->flop[0] = flpCreate(0);
	dif->fdc->flop[1] = flpCreate(1);
	dif->fdc->flop[2] = flpCreate(2);
	dif->fdc->flop[3] = flpCreate(3);
	dif->fdc->flp = dif->fdc->flop[0];
	difSetHW(dif, type);
	return dif;
}

void difDestroy(DiskIF* dif) {
	flpDestroy(dif->fdc->flop[0]);
	flpDestroy(dif->fdc->flop[1]);
	flpDestroy(dif->fdc->flop[2]);
	flpDestroy(dif->fdc->flop[3]);
	dif->fdc->flp = NULL;
	free(dif->fdc);
	free(dif);
}

void difReset(DiskIF* dif) {
	dif->hw->reset(dif);
}

void difSync(DiskIF* dif, int ns) {
	dif->hw->sync(dif, ns);
}

int difOut(DiskIF* dif, int port, int val, int dos) {
	return dif->hw->out(dif,port,val,dos);
}

int difIn(DiskIF* dif, int port, int* res, int dos) {
	return dif->hw->in(dif,port,res,dos);
}
