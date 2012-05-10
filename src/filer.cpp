#include "filer.h"
#include "libxpeccy/spectrum.h"
#include "common.h"

#include "filetypes/filetypes.h"

extern ZXComp* zx;
QFileDialog *filer;
QDir lastDir;

#include <QIcon>
#include <QMessageBox>

void initFileDialog(QWidget*) {
	filer = new QFileDialog();
	filer->setWindowIcon(QIcon(":/images/logo.png"));
	filer->setWindowModality(Qt::ApplicationModal);
	filer->setNameFilterDetailsVisible(true);
	filer->setConfirmOverwrite(true);
	filer->setOptions(QFileDialog::DontUseNativeDialog);
	lastDir = QDir::home();
}

bool saveChangedDisk(int id) {
	bool res=true;
	Floppy* flp = zx->bdi->flop[id];
	if (flp->flag & FLP_CHANGED) {
		QMessageBox mbox;
		mbox.setText(QString("<b>Disk ").append(QChar('A'+id)).append(": has been changed</b>"));
		mbox.setInformativeText("Do you want to save it?");
		mbox.setStandardButtons(QMessageBox::Yes|QMessageBox::Ignore|QMessageBox::Cancel);
		mbox.setIcon(QMessageBox::Warning);
		switch (mbox.exec()) {
			case QMessageBox::Yes: res = saveFile(flp->path,FT_DISK,id); break;		// save
			case QMessageBox::Ignore: res=true; break;					// don't save
			case QMessageBox::Cancel: res=false; break;					// cancel
		}
	}
	return res;
}

QString getFilter(int flags) {
	QString res = "";
	if (flags & FT_SNA) res.append(" *.sna");
	if (flags & FT_Z80) res.append(" *.z80");
	if (flags & FT_TAP) res.append(" *.tap");
	if (flags & FT_TZX) res.append(" *.tzx");
	if (flags & FT_SCL) res.append(" *.scl");
	if (flags & FT_TRD) res.append(" *.trd");
	if (flags & FT_FDI) res.append(" *.fdi");
	if (flags & FT_UDI) res.append(" *.udi");
#ifdef HAVEZLIB
	if (flags & FT_RZX) res.append(" *.rzx");
#endif
	if (flags & FT_HOBETA) res.append(" *.$?");
	if (res.startsWith(" ")) res.remove(0,1);
	return res;
}

int getFileType(QString path) {
	if (path.endsWith(".sna",Qt::CaseInsensitive)) return FT_SNA;
	if (path.endsWith(".z80",Qt::CaseInsensitive)) return FT_Z80;
	if (path.endsWith(".tap",Qt::CaseInsensitive)) return FT_TAP;
	if (path.endsWith(".tzx",Qt::CaseInsensitive)) return FT_TZX;
	if (path.endsWith(".scl",Qt::CaseInsensitive)) return FT_SCL;
	if (path.endsWith(".trd",Qt::CaseInsensitive)) return FT_TRD;
	if (path.endsWith(".fdi",Qt::CaseInsensitive)) return FT_FDI;
	if (path.endsWith(".udi",Qt::CaseInsensitive)) return FT_UDI;
#ifdef HAVEZLIB
	if (path.endsWith(".rzx",Qt::CaseInsensitive)) return FT_RZX;
#endif
	QStringList pspl = path.split(".");
	if (pspl.size() > 0) {
		if (pspl.last().startsWith("$")) return FT_HOBETA;
	}
	return FT_NONE;
}

void loadFile(const char* name, int flags, int drv) {
	QString opath = QDialog::trUtf8(name);
	filer->setDirectory(lastDir);
	if (opath == "") {
		QString filters = "";
		if (drv == -1) filters = QString("All known types (").append(getFilter(flags)).append(")");
		if (flags & FT_DISK) {
			if ((drv == -1) || (drv == 0)) filters.append(";;Disk A (").append(getFilter(flags & (FT_DISK | FT_HOBETA))).append(")");
			if ((drv == -1) || (drv == 1)) filters.append(";;Disk B (").append(getFilter(flags & (FT_DISK | FT_HOBETA))).append(")");
			if ((drv == -1) || (drv == 2)) filters.append(";;Disk C (").append(getFilter(flags & (FT_DISK | FT_HOBETA))).append(")");
			if ((drv == -1) || (drv == 3)) filters.append(";;Disk D (").append(getFilter(flags & (FT_DISK | FT_HOBETA))).append(")");
		}
		if (flags & FT_SNAP) filters.append(";;Snapshot (").append(getFilter(flags & FT_SNAP)).append(")");
		if (flags & FT_TAPE) filters.append(";;Tape (").append(getFilter(flags & FT_TAPE)).append(")");
		if (flags & FT_RZX) filters.append(";;RZX file (").append(getFilter(flags & FT_RZX)).append(")");
		if (flags & FT_RAW) filters.append(";;Raw file to disk A (*.*)");
		if (filters.startsWith(";;")) filters.remove(0,2);
		filer->setWindowTitle("Open file");
		filer->setNameFilter(filters);
		filer->setDirectory(lastDir);
		filer->setAcceptMode(QFileDialog::AcceptOpen);
		if (!filer->exec()) return;
		filters = filer->selectedNameFilter();
		if (filters.contains("Disk A")) drv = 0;
		if (filters.contains("Disk B")) drv = 1;
		if (filters.contains("Disk C")) drv = 2;
		if (filters.contains("Disk D")) drv = 3;
		if (filters.contains("Raw")) drv = 10;
		opath = filer->selectedFiles().first();
		lastDir = filer->directory().absolutePath();
	}
	if (drv == -1) drv = 0;
	int type;
	if (drv == 10) {
		type = FT_RAW;
		drv = 0;
	} else {
		type = getFileType(opath);
	}
	std::string sfnam(opath.toUtf8().data());
	int ferr = ERR_OK;
	zx->rzxPlay = false;
	Floppy* flp = zx->bdi->flop[drv];
	switch (type) {
		case FT_SNA: ferr = loadSNA(zx,sfnam.c_str()); break;
		case FT_Z80: ferr = loadZ80(zx,sfnam.c_str()); break;
		case FT_TAP: ferr = loadTAP(zx->tape,sfnam.c_str()); break;
		case FT_TZX: ferr = loadTZX(zx->tape,sfnam.c_str()); break;
		case FT_SCL: if (saveChangedDisk(drv)) {ferr = loadSCL(flp,sfnam.c_str());} break;
		case FT_TRD: if (saveChangedDisk(drv)) {ferr = loadTRD(flp,sfnam.c_str());} break;
		case FT_FDI: if (saveChangedDisk(drv)) {ferr = loadFDI(flp,sfnam.c_str());} break;
		case FT_UDI: if (saveChangedDisk(drv)) {ferr = loadUDI(flp,sfnam.c_str());} break;
		case FT_HOBETA: ferr = loadHobeta(flp,sfnam.c_str()); break;
		case FT_RAW: ferr = loadRaw(flp,sfnam.c_str()); break;
#if HAVEZLIB
		case FT_RZX: ferr = loadRZX(zx,sfnam.c_str()); break;
#endif
	}
	switch (ferr) {
		case ERR_CANT_OPEN: shitHappens("Can't open file"); break;
		case ERR_RZX_SIGN: shitHappens("Wrong RZX signature"); break;
		case ERR_RZX_CRYPT: shitHappens("Xpeccy cannot into crypted RZX"); break;
		case ERR_RZX_UNPACK: shitHappens("RZX unpack error"); break;
		case ERR_TZX_SIGN: shitHappens("Wrong TZX signature"); break;
		case ERR_TZX_UNKNOWN: shitHappens("Unknown TZX block"); break;
		case ERR_TRD_LEN: shitHappens("Incorrect TRD size"); break;
		case ERR_TRD_SIGN: shitHappens("Not TRDOS disk"); break;
		case ERR_UDI_SIGN: shitHappens("Wrong UDI signature"); break;
		case ERR_FDI_SIGN: shitHappens("Wrong FDI signature"); break;
		case ERR_FDI_HEAD: shitHappens("Wrong FDI heads count"); break;
		case ERR_HOB_CANT: shitHappens("Can't create file at disk"); break;
		case ERR_SCL_SIGN: shitHappens("Wrong SCL signature"); break;
		case ERR_SCL_MANY: shitHappens("Too many files in SCL"); break;
		case ERR_RAW_LONG: shitHappens("File is too big"); break;
	}
}

bool saveFile(const char* name,int flags,int drv) {
	QString path(name);
	QString filters = "";
	if (flags & FT_DISK) {
		if (((drv == -1) || (drv == 0)) && (zx->bdi->flop[0]->flag & FLP_INSERT)) filters.append(";;Disk A (*.scl *.trd *.udi)");
		if ((drv == 1) && (zx->bdi->flop[1]->flag & FLP_INSERT)) filters.append(";;Disk B (*.scl *.trd *.udi)");
		if ((drv == 2) && (zx->bdi->flop[2]->flag & FLP_INSERT)) filters.append(";;Disk C (*.scl *.trd *.udi)");
		if ((drv == 3) && (zx->bdi->flop[3]->flag & FLP_INSERT)) filters.append(";;Disk D (*.scl *.trd *.udi)");
	}
	if (flags & FT_SNAP) filters.append(";;Snapshot (*.sna)");
	if ((flags & FT_TAPE) && (zx->tape->data.size() != 0)) filters.append(";;Tape (*.tap)");
	if (filters.startsWith(";;")) filters.remove(0,2);
	filer->setWindowTitle("Save file");
	filer->setNameFilter(filters);
	filer->setAcceptMode(QFileDialog::AcceptSave);
	filer->setDirectory(lastDir);
	if (path != "") filer->selectFile(path);
	if (!filer->exec()) return false;
	filters = filer->selectedNameFilter();
	if (filters.contains("Disk A")) drv = 0;
	if (filters.contains("Disk B")) drv = 1;
	if (filters.contains("Disk C")) drv = 2;
	if (filters.contains("Disk D")) drv = 3;
	if (drv == -1) drv = 0;
	path = filer->selectedFiles().first();
	lastDir = filer->directory().absolutePath();
	std::string sfnam(path.toUtf8().data());
	int type = getFileType(path);
	int err = ERR_OK;
	if (filters.contains("Disk")) {
		Floppy* flp = zx->bdi->flop[drv];
		switch (type) {
			case FT_SCL: err = saveSCL(flp,sfnam.c_str()); break;
			case FT_TRD: err = saveTRD(flp,sfnam.c_str()); break;
			case FT_UDI: err = saveUDI(flp,sfnam.c_str()); break;
			default: sfnam += ".trd"; err = saveTRD(flp,sfnam.c_str()); break;
		}
	}
	if (filters.contains("Tape")) {
		switch (type) {
			case FT_TAP: err = saveTAP(zx->tape,sfnam.c_str()); break;
			default: sfnam += ".tap"; err = saveTAP(zx->tape,sfnam.c_str()); break;
		}
	}
	if (filters.contains("Snap")) {
		bool mt = (zx->opt.hwName == "ZX48K");
		switch (type) {
			case FT_SNA: err = saveSNA(zx,sfnam.c_str(),mt); break;
			default: sfnam += ".sna"; err = saveSNA(zx,sfnam.c_str(),mt); break;
		}
	}
	switch (err) {
		case ERR_CANT_OPEN: shitHappens("Can't open file"); break;
		case ERR_TRD_SNF: shitHappens("Wrong disk structure for TRD file"); break;
	}
	return true;
}
