I64 SATARep()
{
	I64			 /*bdf, */i, j, num = 0;
	CAHCIPort	*port;
//	CPCIDev		*pci;
	CBlkDev		*temp_blkdev;
	U16			*st, *model, *serial;

	"\nAHCI version %X.%1X%1X\n",
		blkdev.ahci_hba->version >> 16, (blkdev.ahci_hba->version & 0xFF00) >> 8, blkdev.ahci_hba->version & 0xFF;

/*	if (dev.pci_head)
	{
		pci = PCIDevFind(PCIC_STORAGE, PCISC_AHCI);
//		ClassRep(pci);
		"$$PURPLE$$$$HL,1$$Bus: 0x%02X, Dev: 0x%02X, Fun: 0x%02X$$HL,0$$$$FG$$\n\n", pci->bus, pci->dev, pci->fun;
		"$$PURPLE$$Vendor$$FG$$: $$BLACK$$%s$$FG$$\n", pci->vendor_str;
		"$$PURPLE$$Device$$FG$$: $$BLACK$$%s$$FG$$\n", pci->dev_id_str;
	}	
	else
	{
		bdf = PCIClassFind(PCIC_STORAGE << 16 | PCISC_AHCI << 8 + 1, 0);
		"Bus:%02X, Dev:%02X, Fun:%02X", bdf.u8[2], bdf.u8[1], bdf.u8[0];
		"HBA Base Address: 0x%X", PCIReadU32(bdf.u8[2], bdf.u8[1], bdf.u8[0], PCIR_BASE5) & ~0x1F;
	}
*/


	if (blkdev.ahci_hba)
	{
		"\nImplemented Ports:\n\n";
		for (i = 0; i < AHCI_MAX_PORTS; i++)
		{
			if (Bt(&blkdev.ahci_hba->ports_implemented, i))
			{
				port = &blkdev.ahci_hba->ports[i];

				if (port->signature == AHCI_PxSIG_ATAPI || port->signature == AHCI_PxSIG_ATA)
				{
					"$$PURPLE$$ $$BT,\"%d\",LM=\"%d\n\"$$$$FG$$", i, i;

					if (port->signature == AHCI_PxSIG_ATA)
						"$$LM,4$$$$RED$$Hard Drive   $$LTBLUE$$ATA$$FG$$\n";
					else if (port->signature == AHCI_PxSIG_ATAPI)
						"$$LM,4$$$$RED$$CD/DVD Drive $$LTBLUE$$ATAPI$$FG$$\n";
//					else
//						"$$LM,4$$$$RED$$Unknown      $$LTBLUE$$0x%0X$$FG$$\n", port->signature;

					"$$LM,0$$\n\t";

					temp_blkdev = CAlloc(sizeof(CBlkDev));
					AHCIPortInit(temp_blkdev, port, i);
					"\n\t";

					if (temp_blkdev->dev_id_record)
					{
						st = CAlloc(40 + 1);
						for (j = 0; j < 20; j++)
							st[j] = EndianU16(temp_blkdev->dev_id_record[27 + j]);
						model = MStrUtil(st, SUF_REM_LEADING | SUF_REM_TRAILING);
						"Model:  %s\n\t", model;
						Free(st);
						Free(model);

						st = CAlloc(20 + 1);
						for (j = 0; j < 10; j++)
							st[j] = EndianU16(temp_blkdev->dev_id_record[10 + j]);
						serial = MStrUtil(st, SUF_REM_LEADING | SUF_REM_TRAILING);
						"Serial: %s\n", serial;
						Free(st);
						Free(serial);
					}

					"\n";

					BlkDevDel(temp_blkdev);
					Free(temp_blkdev);
				}
				num++;
			}
		}
	}
	else
		"$$HL,1$$blkdev.ahci_hba$$HL,0$$ is NULL !\n\n";

	return num;
}

#help_index "Install;File/Cmd Line (Typically);Cmd Line (Typically);"
U8 Mount2(U8 boot_drive_let, CDoc *_doc, Bool _caller_is_prtdisk)
{//If _doc, called by $LK,"::/Kernel/KConfig.CC"$ else called by $LK,"Mount",A="MN:Mount"$().
	I64		 count, total = 0, num_hints, drv_let, type, unit, prt_num, port = -1; // ahci
	U8		 blks_buf[STR_LEN], addr_buf[STR_LEN], base0_buf[STR_LEN],// base1_buf[STR_LEN],
			*filename = NULL, *filename2 = NULL, res = 0;
//	CATARep	*head = NULL, *tmpha;
	Bool	 whole_drive, make_free;
	CDoc	*doc;

	if (boot_drive_let)
		boot_drive_let = Letter2Letter(boot_drive_let);
	do
	{
		count = 0;
		if (!_doc)
			DriveRep;

		"\n****** Mount Drives ******\n"
		"$$GREEN$$A$$FG$$-$$GREEN$$B$$FG$$ are RAM drives.\n"
		"$$GREEN$$C$$FG$$-$$GREEN$$L$$FG$$ are ATA hard drives.\n"
		"$$GREEN$$M$$FG$$-$$GREEN$$P$$FG$$ are ISO file read drives.\n"
		"$$GREEN$$Q$$FG$$-$$GREEN$$S$$FG$$ are ISO file write drives.\n"
		"$$GREEN$$T$$FG$$-$$GREEN$$Z$$FG$$ are ATAPI CD/DVD drives.\n"
		"\nDrive Letter ($$PURPLE$$<ENTER>$$FG$$ to exit):";
		drv_let = Letter2Letter(CharGet);
		'\n';
		if (type = Letter2BlkDevType(drv_let))
		{
			whole_drive = FALSE;
			if (_doc)
			{ //Called by $LK,"::/Kernel/KConfig.CC"$
				doc = _doc;
				make_free = FALSE;
			}
			else
			{ //Called by $LK,"Mount",A="MN:Mount"$()
				doc = DocNew;
				DocPrint(doc, "CBlkDev *bd;\n");
				make_free = TRUE;
			}
			unit = 0;
			prt_num = I64_MIN;
			switch (type)
			{
				case BDT_RAM:
					"Addr of RAM disk ($$PURPLE$$<ENTER>$$FG$$ to MAlloc):";
					StrNGet(addr_buf, STR_LEN);
				case BDT_ISO_FILE_WRITE:
					"Blks of 512 bytes:";
					StrNGet(blks_buf, STR_LEN);
					break;

				case BDT_ISO_FILE_READ:
					filename = StrGet("File Name:");
					break;

				case BDT_ATA:
					prt_num = I64Get("Partition Num (Default=All):", prt_num);
				case BDT_ATAPI:
//					num_hints = ATARep(,, &head);
					num_hints = SATARep; // ahci
					if (type == BDT_ATAPI && boot_drive_let)
						"<ENTER> to use booted CD/DVD\n"; //Only $LK,"::/Kernel/KConfig.CC"$
					do
					{
						if (num_hints)
							"Enter port number: \n"; // ahci (!! using base0_buf for port num !!)
//							"Enter dev number or\nport with $$PURPLE$$0x$$FG$$ prefix.\n"
//							"I/O Port Base0:\n";
//						else
//							"Include $$PURPLE$$0x$$FG$$ prefix.\nI/O Port Base0:\n";
						StrNGet(base0_buf, STR_LEN);
					}
//					while (!Str2I64(base0_buf) && (type != BDT_ATAPI || !boot_drive_let));
					while ((0 > Str2I64(base0_buf) || Str2I64(base0_buf) > num_hints - 1) &&
							(type != BDT_ATAPI || !boot_drive_let)); // ahci
					port = Str2I64(base0_buf);
					
/*					if (1 <= Str2I64(base0_buf) <= num_hints)
					{
						tmpha = ATARepFind(head, Str2I64(base0_buf));
						StrPrint(base0_buf, "0x%X", tmpha->base0);
						StrPrint(base1_buf, "0x%X", tmpha->base1);
						unit = tmpha->unit;
					}
					else if (type != BDT_ATAPI || *base0_buf)
					{
						if (type == BDT_ATAPI)
							StrCopy(base1_buf, "0");
						else
							do
							{
								"I/O Port Base1:\n";
								StrNGet(base1_buf, STR_LEN);
							}
							while (!Str2I64(base1_buf));
						do
						{
							"\t$$PURPLE$$0$$FG$$=Master\n\t$$PURPLE$$1$$FG$$=Slave\nUnit:";
							unit = CharGet - '0';
						}
						while (!(0 <= unit <= 1));
						'\n';
					}
					LinkedListDel(head);*/
					break;
			}
			DocPrint(doc, "\"bd = BlkDevNextFreeSlot('%C', %d); bd->unit = %d;\n\";\n", drv_let, type, unit);
			DocPrint(doc, "bd = BlkDevNextFreeSlot(\'%C\', %d); bd->unit = %d;\n", drv_let, type, unit);
			if (port != -1 && *base0_buf) // ahci
			{
				DocPrint(doc, "\"AHCIPortInit(bd, &blkdev.ahci_hba->ports[%d], %d);\n\";\n", port, port);
				DocPrint(doc, "AHCIPortInit(bd, &blkdev.ahci_hba->ports[%d], %d);\n", port, port);

			}

			switch (type)
			{
				case BDT_RAM:
					if (!*addr_buf) StrCopy(addr_buf, "0");
					DocPrint(doc, "bd->RAM_disk = %s;\n", addr_buf);
				case BDT_ISO_FILE_WRITE:
					if (!*blks_buf) StrCopy(blks_buf, "0");
					DocPrint(doc, "bd->max_blk = (%s) - 1;\n", blks_buf);
					DocPrint(doc, "bd->drv_offset = 19 << 2 + (DVD_BLK_SIZE * 2 + DVD_BOOT_LOADER_SIZE) / BLK_SIZE;\n");
					break;

				case BDT_ISO_FILE_READ:
					filename2 = FileNameAbs(filename);
					DocPrint(doc, "bd->file_disk_name = ZStrNew(\"%s\");\n", filename2);
					DocPrint(doc, "bd->drv_offset = 19 << 2 + (DVD_BLK_SIZE * 2 + DVD_BOOT_LOADER_SIZE) / BLK_SIZE;\n");
					break;

				case BDT_ATA:
				case BDT_ATAPI:
					if (type == BDT_ATAPI && !*base0_buf && _doc)
					{
//						DocPrint(doc, "\"GetBaseUnit(bd);\n\";\n");
//						DocPrint(doc, "GetBaseUnit(bd);\n"); //Only $LK,"::/Kernel/KConfig.CC"$
						DocPrint(doc, "\"AHCIBootDVDProbeAll(bd);\n\";\n");
						DocPrint(doc, "AHCIBootDVDProbeAll(bd);\n"); //Only $LK,"::/Kernel/KConfig.CC"$

						if (drv_let == boot_drive_let)
							make_free = TRUE;
					}
//					else
//						DocPrint(doc, "bd->base0 = %s; bd->base1 = %s;\n", base0_buf, base1_buf);
					if (type == BDT_ATA && _caller_is_prtdisk)
					{
						"\nReformat WHOLE drive!";
						whole_drive = YorN;
					}
					break;
			}
			DocPrint(doc, "\"BlkDevAdd(bd, %d, %d, %d);\n\";\n", prt_num, whole_drive, make_free);
			DocPrint(doc, "BlkDevAdd(bd, %d, %d, %d);\n", prt_num, whole_drive, make_free);
			if (_doc) //Called by $LK,"::/Kernel/KConfig.CC"$
				count++;
			else
			{ //Called by $LK,"Mount",A="MN:Mount"$()
				if ((count = ExeDoc(doc)) && whole_drive)
				{
					if (_caller_is_prtdisk)
					{
						res = drv_let;
						DiskPart(drv_let, 1.0); //First mount whole drive.
					}
					else
						DiskPart(drv_let);
				}
				DocDel(doc);
			}
		}
		total += count;
	}
	while (count && !_caller_is_prtdisk || !total && _doc); //At least 1 if Called by $LK,"::/Kernel/KConfig.CC"$

	Free(filename);
	Free(filename2);

	return res;
}

public U8 Mount(Bool caller_is_prtdisk=FALSE)
{//Mount drives. Called from $LK,"DiskPart(Mount)",A="FF:::/Zenith/ZBlkDev/DiskPart.CC,Mount("$.
	return Mount2(0, NULL, caller_is_prtdisk);
}

public U0 Unmount(U8 drv_let=0)
{//Unmount drive(s).
	BlkDevDel(Letter2BlkDev(drv_let));
}

public U8 MountFile(U8 *filename)
{//Mount ISO.C file.
	U8		*filename2 = ExtDefault(filename, "ISO.C"), *filename3 = FileNameAbs(filename2);
	CDrive	*drive = DriveMakeFreeSlot(DriveNextFreeLet('M')); //First $LK,"BDT_ISO_FILE_READ",A="MN:BDT_ISO_FILE_READ"$
	CBlkDev	*bd = BlkDevNextFreeSlot(drive->drv_let, BDT_ISO_FILE_READ);

	bd->drv_offset = 19 << 2 + (DVD_BLK_SIZE * 2 + DVD_BOOT_LOADER_SIZE) / BLK_SIZE;
	bd->file_disk_name = ZStrNew(filename3);
	BlkDevAdd(bd,, TRUE, TRUE);
	Free(filename3);
	Free(filename2);

	return drive->drv_let;
}
