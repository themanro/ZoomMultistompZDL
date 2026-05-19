
TEXT Section .text (Little Endian), 0x1820 bytes at 0x00000000
00000000            SUB_KawaOD_EP_Booster:
00000000       25f7           STW.D2T1      A11,*B15--[2]
00000002       2577           STW.D2T1      A10,*B15--[2]
00000004       9677           STDW.D2T2     B13:B12,*B15--[1]
00000006       9577           STDW.D2T2     B11:B10,*B15--[1]
00000008       07a7 ||        MVK.L2        0,B7
0000000a       8777           STDW.D2T1     A15:A14,*B15--[1]
0000000c   039efd8a ||        SET.S2        B7,23,29,B7
00000010   0200e22b           MVK.S2        0x01c4,B4
00000014       8677 ||        STDW.D2T1     A13:A12,*B15--[1]
00000016       57c6           MV.L1X        B7,A10
00000018   0380e02b ||        MVK.S2        0x01c0,B7
0000001c   e4e00828           .fphead       n, l, W, BU, nobr, nosat, 0100111b
00000020   01bed4f7 ||        STW.D2T2      B3,*B15--[22]
00000024       9341 ||        ADD.L2X       B4,A6,B4
00000026       f341           ADD.L2X       B7,A6,B4
00000028       9e45 ||        STW.D2T2      B4,*B15[16]
0000002a       fdc5           STW.D2T2      B4,*B15[15]
0000002c   0200e82a ||        MVK.S2        0x01d0,B4
00000030   0200ec2b           MVK.S2        0x01d8,B4
00000034   0618907a ||        ADD.L2X       B4,A6,B12
00000038   0200f42b           MVK.S2        0x01e8,B4
0000003c   e0c00028           .fphead       n, l, W, BU, nobr, nosat, 0000110b
00000040   0f98907a ||        ADD.L2X       B4,A6,B31
00000044   0200fc2b           MVK.S2        0x01f8,B4
00000048       9331 ||        ADD.L2X       B4,A6,B3
0000004a       05a6 ||        MVK.L1        0,A3
0000004c   0201082b           MVK.S2        0x0210,B4
00000050   0518907b ||        ADD.L2X       B4,A6,B10
00000054   01dfc068 ||        MVKH.S1       0xbf800000,A3
00000058       9341           ADD.L2X       B4,A6,B4
0000005a       ddc5           STW.D2T2      B4,*B15[14]
0000005c   e8802010           .fphead       n, l, W, BU, nobr, nosat, 1000100b
00000060   0200d02b ||        MVK.S2        0x01a0,B4
00000064   08009029 ||        MVK.S1        0x0120,A16
00000068       5dc7 ||        MV.L2X        A3,B26
0000006a       8a46           MV.L1         A4,A20
0000006c   0698907a ||        ADD.L2X       B4,A6,B13
00000070   0241105b           ADD.L2X       8,A16,B4
00000074   0186b428 ||        MVK.S1        0x0d68,A3
00000078   0250907b           ADD.L2X       B4,A20,B4
0000007c   e0800020           .fphead       n, l, W, BU, nobr, nosat, 0000100b
00000080   0d98a2e7 ||        LDW.D2T2      *+B6[5],B27
00000084   01c00068 ||        MVKH.S1       0x80000000,A3
00000088       fd45           STW.D2T2      B4,*B15[11]
0000008a       0f46 ||        MV.L1         A6,A24
0000008c   078c0265 ||        LDW.D1T1      *+A3[0],A15
00000090   04009c2a ||        MVK.S2        0x0138,B8
00000094   041962e7           LDW.D2T2      *+B6[11],B8
00000098   0251107b ||        ADD.L2X       B8,A20,B4
0000009c   e0800030           .fphead       n, l, W, BU, nobr, nosat, 0000100b
000000a0   0380e42b ||        MVK.S2        0x01c8,B7
000000a4   01e19ec0 ||        ADDAD.D1      A24,0xc,A3
000000a8   0598f07b           ADD.L2X       B7,A6,B11
000000ac   0381042b ||        MVK.S2        0x0208,B7
000000b0       ce35 ||        STW.D2T1      A3,*B15[18]
000000b2       997d           LDW.D2T2      *B6[12],B7
000000b4   0f18f07b ||        ADD.L2X       B7,A6,B30
000000b8   04809a2a ||        MVK.S2        0x0134,B9
000000bc   e2000200           .fphead       n, l, W, BU, nobr, nosat, 0010000b
000000c0   0251307b           ADD.L2X       B9,A20,B4
000000c4       dd45 ||        STW.D2T2      B4,*B15[10]
000000c6       bd45           STW.D2T2      B4,*B15[9]
000000c8   01a002e5           LDW.D2T1      *+B8[0],A3
000000cc   0327805b ||        SUB.L2        B9,0x4,B6
000000d0   0380f029 ||        MVK.S1        0x01e0,A7
000000d4   029016a2 ||        MV.S2X        A4,B5
000000d8   02153d43           ADDAW.D2      B5,0x9,B4
000000dc   e0400000           .fphead       n, l, W, BU, nobr, nosat, 0000010b
000000e0   0350d07b ||        ADD.L2X       B6,A20,B6
000000e4   02810028 ||        MVK.S1        0x0200,A5
000000e8   0880e029           MVK.S1        0x01c0,A17
000000ec       9d65 ||        STW.D2T2      B6,*B15[8]
000000ee       9dc5           STW.D2T2      B4,*B15[12]
000000f0   04009628 ||        MVK.S1        0x012c,A8
000000f4   02178943           ADD.D2        B5,0x1c,B4
000000f8   0580a359 ||        MVK.L1        0,A11
000000fc   e1000080           .fphead       n, l, W, BU, nobr, nosat, 0001000b
00000100   0900d828 ||        MVK.S1        0x01b0,A18
00000104   04c08941           ADD.D1        A16,0x4,A9
00000108   04c3905b ||        SUB.L2X       A16,0x4,B9
0000010c   0c98e079 ||        ADD.L1        A7,A6,A25
00000110   0e9201e1 ||        ADD.S1        A16,A4,A29
00000114       ccb5 ||        STW.D2T1      A3,*B15[6]
00000116       bdc5           STW.D2T2      B4,*B15[13]
00000118   02d1307b ||        ADD.L2X       B9,A20,B5
0000011c   e4000800           .fphead       n, l, W, BU, nobr, nosat, 0100000b
00000120       9f13 ||        MVK.S2        156,B6
00000122       a310 ||        ADD.L1        A5,A6,A1
00000124   05a04069 ||        MVKH.S1       0x40800000,A11
00000128   0144c840 ||        ADD.D1        A17,A6,A2
0000012c   0da28841           ADD.D1        A8,A20,A27
00000130   0250d07b ||        ADD.L2X       B6,A20,B4
00000134   0e512079 ||        ADD.L1        A9,A20,A28
00000138   0200c829 ||        MVK.S1        0x0190,A4
0000013c   e0200003           .fphead       n, l, W, BU, nobr, nosat, 0000001b
00000140       fcd5 ||        STW.D2T2      B5,*B15[7]
00000142       be45           STW.D2T2      B4,*B15[17]
00000144   0710c841 ||        ADD.D1        A4,A6,A14
00000148   0300f829 ||        MVK.S1        0x01f0,A6
0000014c   069a4079 ||        ADD.L1        A18,A6,A13
00000150       0ea7 ||        MVK.L2        8,B5
00000152       fe55           STW.D2T2      B5,*B15[19]
00000154   0d60c079 ||        ADD.L1        A6,A24,A26
00000158   0b1f91a1 ||        SUB.S1X       B7,0x4,A22
0000015c   e2200202           .fphead       n, l, W, BU, nobr, nosat, 0010001b
00000160   06000040 ||        MVK.D1        0,A12
00000164       06a6           MVK.L1        0,A5
00000166       cccd           LDW.D2T1      *B15[6],A4
00000168   0296bd88 ||        SET.S1        A5,21,29,A5
0000016c            $C$L2:
0000016c   01d83264           LDW.D1T1      *++A22[1],A3
00000170       6c6e           NOP           4
00000172       0034           STW.D1T1      A3,*A4[0]
00000174   026c02e6           LDW.D2T2      *+B27[0],B4
00000178   01ed02e4           LDW.D2T1      *+B27[8],A3
0000017c   e2400008           .fphead       n, l, W, BU, nobr, nosat, 0010010b
00000180       0626           MVK.L1        0,A4
00000182       4c6e           NOP           3
00000184   01907218           ADDSP.L1X     A3,B4,A3
00000188   00004000           NOP           3
0000018c   031065b0           MPYSPDP.M1    A3,A5:A4,A7:A6
00000190       8dbd           LDW.D2T1      *B15[12],A3
00000192       a626           MVK.L1        5,A4
00000194       2c6e           NOP           2
00000196       4c67           SPLOOPD       9
00000198   069013a3 ||        MVC.S2X       A4,ILC
0000019c   e6200800           .fphead       n, l, W, BU, nobr, nosat, 0110001b
000001a0   02600fd8 ||        MV.L1         A24,A4
000001a4       2c1c           LDNDW.D1T1    *A4++[2],A17:A16
000001a6       2de7           SPMASK        L1,L2,S1
000001a8   040d905b ||^       ADD.L2X       12,A3,B8
000001ac   028d01a1 ||^       ADD.S1        8,A3,A5
000001b0   0b9cc139 ||^       DPSP.L1       A7:A6,A23
000001b4   030e9724 ||        LDNDW.D1T1    *A3++(20),A7:A6
000001b8   0814b664           LDW.D1T1      *A5++[5],A16
000001bc   e04c0008           .fphead       n, h, DW/NDW, W, nobr, nosat, 0000010b
000001c0       0c6e           NOP           1
000001c2       ac66           SPMASK        D2
000001c4   0ebd42e6 ||^       LDW.D2T2      *+B15[10],B29
000001c8       ac66           SPMASK        D2
000001ca       9d5d ||^       LDW.D2T2      *B15[8],B5
000001cc   008b0001           SPMASK        L2,D2
000001d0   0abce2e7 ||^       LDW.D2T2      *+B15[7],B21
000001d4   0261105b ||^       ADD.L2X       8,A24,B4
000001d8   0340ee00 ||        MPYSP.M1      A7,A16,A6
000001dc   e0a00012           .fphead       n, l, W, BU, nobr, nosat, 0000101b
000001e0   039057a6           LDNDW.D2T2    *B4++[2],B7:B6
000001e4   092297a7           LDNDW.D2T2    *B8++(20),B19:B18
000001e8   08460e00 ||        MPYSP.M1      A16,A17,A16
000001ec   091b40f0           MVD.M1        A6,A18
000001f0       0c6e           NOP           1
000001f2       ac66           SPMASK        D2
000001f4   04bd22e6 ||^       LDW.D2T2      *+B15[9],B9
000001f8       ac66           SPMASK        D2
000001fa       fd0d ||^       LDW.D2T2      *B15[11],B16
000001fc   ea083200           .fphead       n, h, W, BU, nobr, nosat, 1010000b
00000200   039a0218 ||        ADDSP.L1      A16,A6,A7
00000204   00830001           SPMASK        D2
00000208   0bd402e7 ||^       LDW.D2T2      *+B21[0],B23
0000020c   031a4e02 ||        MPYSP.M2      B18,B6,B6
00000210   00470001           SPMASK        L1,D1
00000214   04e00fd9 ||^       MV.L1         A24,A9
00000218   0e6c0267 ||        LDW.D1T2      *+A27[0],B28
0000021c   089e6e02 ||        MPYSP.M2      B19,B7,B17
00000220   00830001           SPMASK        D2
00000224   0c1402e7 ||^       LDW.D2T2      *+B5[0],B24
00000228   03a49664 ||        LDW.D1T1      *A9++[4],A7
0000022c   00000000           NOP
00000230   0a1cd21a           ADDSP.L2X     B6,A7,B20
00000234       2c6e           NOP           2
00000236       ae66           SPMASK        S2,D2
00000238   0cc002e7 ||^       LDW.D2T2      *+B16[0],B25
0000023c   e4000800           .fphead       n, l, W, BU, nobr, nosat, 0100000b
00000240       bb8f ||^       MV.S2X        A23,B5
00000242       ec66           SPMASK        D1,D2
00000244   0ff00265 ||^       LDW.D1T1      *+A28[0],A31
00000248   0b2402e7 ||^       LDW.D2T2      *+B9[0],B22
0000024c   02d2221b ||        ADDSP.L2      B17,B20,B5
00000250   0348be02 ||        MPYSP.M2X     B5,A18,B6
00000254       ed66           SPMASK        S1,D1,D2
00000256       d2c6 ||        MV.L1X        B5,A6
00000258   0f740265 ||^       LDW.D1T1      *+A29[0],A30
0000025c   e4200c02           .fphead       n, l, W, BU, nobr, nosat, 0100001b
00000260   0af402e7 ||^       LDW.D2T2      *+B29[0],B21
00000264   046006a0 ||^       MV.S1         A24,A8
00000268   000b0001           SPMASK        L2
0000026c   0861105b ||^       ADD.L2X       8,A24,B16
00000270   03a05774 ||        STNDW.D1T1    A7:A6,*A8++[2]
00000274   09c096e6           LDW.D2T2      *B16++[4],B19
00000278   00230001           SPMASK        S2
0000027c   04e111a3 ||^       ADD.S2X       8,A24,B9
00000280   0294c21a ||        ADDSP.L2      B6,B5,B5
00000284       4c6e           NOP           3
00000286       4ad7           MV.D2         B5,B18
00000288   00034001           SPKERNEL      0,0
0000028c   09a457f6 ||        STNDW.D2T2    B19:B18,*B9++[2]
00000290   0e8ca35a           MVK.L2        3,B29
00000294       9697           MV.D2X        A13,B4
00000296       7686 ||        MV.L1X        B13,A3
00000298   06f403a2 ||        MVC.S2        B29,ILC
0000029c   e4400c00           .fphead       n, l, W, BU, nobr, nosat, 0100010b
000002a0   02bd1058           ADD.L1X       8,B15,A5
000002a4   00006000           NOP           4
000002a8   04bd1058           ADD.L1X       8,B15,A9
000002ac   00006000           NOP           4
000002b0   08bd01a2           ADD.S2        8,B15,B17
000002b4       0c6e           NOP           1
000002b6       c686           MV.L1         A13,A6
000002b8       e70e ||        MV.S1         A14,A7
000002ba       cc6e           NOP           7
000002bc   ec000800           .fphead       n, l, W, BU, nobr, nosat, 1100000b
000002c0   033da2e6           LDW.D2T2      *+B15[13],B6
000002c4   08d08264           LDW.D1T1      *+A20[4],A17
000002c8       4c6e           NOP           3
000002ca       117d           LDNDW.D2T2    *B6(0),B7:B6
000002cc   00006000           NOP           4
000002d0   0318ae02           MPYSP.M2      B5,B6,B6
000002d4   021efe00           MPYSP.M1X     A23,B7,A4
000002d8   0386e02a           MVK.S2        0x0dc0,B7
000002dc   e0840000           .fphead       n, l, DW/NDW, W, nobr, nosat, 0000100b
000002e0   03c0006a           MVKH.S2       0x80000000,B7
000002e4   00000000           NOP
000002e8   0a989218           ADDSP.L1X     A4,B6,A21
000002ec       0f67           SPLOOPD       7
000002ee       b707 ||        MV.L2X        A14,B5
000002f0       93c6 ||        MV.L1X        B7,A4
000002f2       2ee7           SPMASK        L1,L2,S2
000002f4       07c7 ||^       MV.L2         B7,B8
000002f6       1bc6 ||^       MV.L1X        B7,A16
000002f8   091436e7 ||        LDW.D2T2      *B5++[1],B18
000002fc   e7000ec0           .fphead       n, l, W, BU, nobr, nosat, 0111000b
00000300   03800852 ||^       ADDK.S2       16,B7
00000304   081c36e6           LDW.D2T2      *B7++[1],B16
00000308   01030001           SPMASK        M1
0000030c   0ac6ae01 ||^       MPYSP.M1      A21,A17,A21
00000310   04103664 ||        LDW.D1T1      *A4++[1],A8
00000314       0c6e           NOP           1
00000316       2ce6           SPMASK        L2
00000318   04b40fdb ||^       MV.L2         B13,B9
0000031c   e4000800           .fphead       n, l, W, BU, nobr, nosat, 0100000b
00000320   040c3664 ||        LDW.D1T1      *A3++[1],A8
00000324   00130001           SPMASK        S1
00000328   08001051 ||^       ADDK.S1       32,A16
0000032c   092436f6 ||        STW.D2T2      B18,*B9++[1]
00000330       2c67           SPMASK        L1
00000332       2ac6 ||^       MV.L1         A21,A17
00000334   09403665 ||        LDW.D1T1      *A16++[1],A18
00000338   08424e02 ||        MPYSP.M2      B18,B16,B16
0000033c   e2080300           .fphead       n, h, W, BU, nobr, nosat, 0010000b
00000340   09a22e00           MPYSP.M1      A17,A8,A19
00000344       0c6e           NOP           1
00000346       1c0d           LDW.D2T2      *B4++[1],B16
00000348   00230001           SPMASK        S2
0000034c   04001853 ||^       ADDK.S2       48,B8
00000350   04183674 ||        STW.D1T1      A8,*A6++[1]
00000354   032036e7           LDW.D2T2      *B8++[1],B6
00000358   04490e01 ||        MPYSP.M1      A8,A18,A8
0000035c   e0480000           .fphead       n, h, W, BU, nobr, nosat, 0000010b
00000360   09427218 ||        ADDSP.L1X     A19,B16,A18
00000364       4c6e           NOP           3
00000366       0ca4           STW.D1T1      A18,*A5++[1]
00000368   04490218 ||        ADDSP.L1      A8,A18,A8
0000036c   0340ce02           MPYSP.M2      B6,B16,B6
00000370   00002000           NOP           2
00000374   04243674           STW.D1T1      A8,*A9++[1]
00000378   0320d21a           ADDSP.L2X     B6,A8,B6
0000037c   e0480008           .fphead       n, h, W, BU, nobr, nosat, 0000010b
00000380       0d94           STW.D1T1      A17,*A7++[1]
00000382       0c6e           NOP           1
00000384       0c6e           NOP           1
00000386       1c66           SPKERNEL      0,0
00000388   034436f6 ||        STW.D2T2      B6,*B17++[1]
0000038c       6c07           MV.L2         B24,B19
0000038e       8b4f ||        MV.S2         B22,B20
00000390   02e808f3 ||        MV.D2         B26,B5
00000394   022806a0 ||        MV.S1         A10,A4
00000398       4bc7           MV.L2         B23,B18
0000039a       a8ce ||        MV.S1         A17,A21
0000039c   e96830c8           .fphead       n, h, W, BU, nobr, nosat, 1001011b
000003a0       ec8f ||        MV.S2         B25,B7
000003a2       3557 ||        MV.D2X        A2,B9
000003a4       0c6e           NOP           1
000003a6       6627           MVK.L2        3,B4
000003a8       cf8e           MV.S1         A31,A6
000003aa       da6f ||        MVC.S2        B4,ILC
000003ac   04700fda           MV.L2         B28,B8
000003b0       4c6e           NOP           3
000003b2       af06           MV.L1         A30,A5
000003b4   083016a2 ||        MV.S2X        A12,B16
000003b8   00008000           NOP           5
000003bc   e2e00211           .fphead       n, l, W, BU, nobr, nosat, 0010111b
000003c0   03bd1058           ADD.L1X       8,B15,A7
000003c4       0c6e           NOP           1
000003c6       d587           MV.L2X        A11,B6
000003c8   0403a000           SPLOOPD       9
000003cc   0c1c3666           LDW.D1T2      *A7++[1],B24
000003d0   0260ce02           MPYSP.M2      B6,B24,B4
000003d4   00004000           NOP           3
000003d8   0cc08ea3           CMPLTSP.S2    B4,B16,B25
000003dc   e0500000           .fphead       p, l, W, BU, nobr, nosat, 0000010b
000003e0   0b124e02 ||        MPYSP.M2      B18,B4,B22
000003e4   00002000           NOP           2
000003e8   006407b2           ROTL.M2       B25,0x0,B0
000003ec   08d90e02           MPYSP.M2      B8,B22,B17
000003f0   00000000           NOP
000003f4   28daae02    [ B0]  MPYSP.M2      B21,B22,B17
000003f8   2bda6e02    [ B0]  MPYSP.M2      B19,B22,B23
000003fc   00000000           NOP
00000400   04d8de00           MPYSP.M1X     A6,B22,A9
00000404   0458be01           MPYSP.M1X     A5,B22,A8
00000408   00923e62 ||        CMPGTSP.S2X   B17,A4,B1
0000040c   008340f3           MVD.M2        B0,B1
00000410   48901fda || [ B1]  MV.L2X        A4,B17
00000414   00162ea2           CMPLTSP.S2    B17,B5,B0
00000418   28940fda    [ B0]  MV.L2         B5,B17
0000041c   0c440fdb           MV.L2         B17,B24
00000420   00110e60 ||        CMPGTSP.S1    A8,A4,A0
00000424   d1a006a1    [!A0]  MV.S1         A8,A3
00000428   c19008f1 || [ A0]  MV.D1         A4,A3
0000042c   0224f21b ||        ADDSP.L2X     B7,A9,B4
00000430   041232b8 ||        SUBSP.L1X     B17,A4,A8
00000434   425e821a    [ B1]  ADDSP.L2      B20,B23,B4
00000438   00147ea0           CMPLTSP.S1X   A3,B5,A0
0000043c   00000000           NOP
00000440   0ce340f3           MVD.M2        B24,B25
00000444   092000a0 ||        SPDP.S1       A8,A19:A18
00000448   c1941fd8    [ A0]  MV.L1X        B5,A3
0000044c   084e4b20           ABSDP.S1      A19:A18,A17:A16
00000450   00909e62           CMPGTSP.S2X   B4,A4,B1
00000454   421016a3    [ B1]  MV.S2X        A4,B4
00000458   084065b0 ||        MPYSPDP.M1    A3,A17:A16,A17:A16
0000045c   01148ea2           CMPLTSP.S2    B4,B5,B2
00000460   621406a2    [ B2]  MV.S2         B5,B4
00000464   0d132e02           MPYSP.M2      B25,B4,B26
00000468   00004000           NOP           3
0000046c   04460138           DPSP.L1       A17:A16,A8
00000470   00004000           NOP           3
00000474   04691218           ADDSP.L1X     A8,B26,A8
00000478       2c6e           NOP           2
0000047a       0c6e           NOP           1
0000047c   e8000000           .fphead       n, l, W, BU, nobr, nosat, 1000000b
00000480   00034001           SPKERNEL      0,0
00000484   0424b6f4 ||        STW.D2T1      A8,*B9++[5]
00000488   09012043           MVK.D2        9,B18
0000048c       4656 ||        MV.D1         A4,A10
0000048e       0c6e           NOP           1
00000490   06c803a2           MVC.S2        B18,ILC
00000494   059818f0           MV.D1X        B6,A11
00000498   06401fd8           MV.L1X        B16,A12
0000049c   e1000000           .fphead       n, l, W, BU, nobr, nosat, 0001000b
000004a0   00010000           NOP           9
000004a4   00002000           NOP           2
000004a8   0280e228           MVK.S1        0x01c4,A5
000004ac       0c6e           NOP           1
000004ae       3456           MV.D1X        B16,A9
000004b0   00006000           NOP           4
000004b4   0206b428           MVK.S1        0x0d68,A4
000004b8   02400068           MVKH.S1       0x80000000,A4
000004bc   e1080000           .fphead       n, h, W, BU, nobr, nosat, 0001000b
000004c0   00008000           NOP           5
000004c4   08e0a1e1           ADD.S1        A5,A24,A17
000004c8   08108940 ||        ADD.D1        A4,0x4,A16
000004cc       4c6e           NOP           3
000004ce       4ec7           MV.L2         B5,B26
000004d0       6c6e           NOP           4
000004d2       0de7           SPLOOPD       4
000004d4   01880264 ||        LDW.D1T1      *+A2[0],A3
000004d8   03c43724           LDNDW.D1T1    *A17++[1],A7:A6
000004dc   e3000200           .fphead       n, l, W, BU, nobr, nosat, 0011000b
000004e0   02c03724           LDNDW.D1T1    *A16++[1],A5:A4
000004e4   00002000           NOP           2
000004e8   01030001           SPMASK        M1
000004ec   040dee00 ||^       MPYSP.M1      A15,A3,A8
000004f0   00000000           NOP
000004f4   01988e00           MPYSP.M1      A4,A6,A3
000004f8   019cae00           MPYSP.M1      A5,A7,A3
000004fc   00002000           NOP           2
00000500   04a46218           ADDSP.L1      A3,A9,A9
00000504   00034001           SPKERNEL      0,0
00000508   04206218 ||        ADDSP.L1      A3,A8,A8
0000050c       a4a6           MVK.L1        5,A17
0000050e       e852 ||        MVK.S1        79,A16
00000510   0a8c03a6 ||        LDNDW.D2T2    *+B3[0],B21:B20
00000514   0bb003a7           LDNDW.D2T2    *+B12[0],B23:B22
00000518   09802828 ||        MVK.S1        0x0050,A19
0000051c   e10800c0           .fphead       n, h, W, BU, nobr, nosat, 0001000b
00000520   06c413a3           MVC.S2X       A17,ILC
00000524   033de2e7 ||        LDW.D2T2      *+B15[15],B6
00000528   0fd0c264 ||        LDW.D1T1      *+A20[6],A31
0000052c   02e80324           LDNDW.D1T1    *+A26[0],A5:A4
00000530   08fc03a6           LDNDW.D2T2    *+B31[0],B17:B16
00000534   03e40325           LDNDW.D1T1    *+A25[0],A7:A6
00000538       5ace ||        MV.S1X        B21,A18
0000053a       ab47           MV.L2         B22,B21
0000053c   e8082000           .fphead       n, h, W, BU, nobr, nosat, 1000000b
00000540   00520a65 ||        LDW.D1T1      *+A20[A16],A0
00000544   0e3dc2e6 ||        LDW.D2T2      *+B15[14],B28
00000548   02ac03a6           LDNDW.D2T2    *+B11[0],B5:B4
0000054c   04a803a6           LDNDW.D2T2    *+B10[0],B9:B8
00000550   00f803a6           LDNDW.D2T2    *+B30[0],B1:B0
00000554   089806a0           MV.S1         A6,A17
00000558   03212218           ADDSP.L1      A9,A8,A6
0000055c   04840324           LDNDW.D1T1    *+A1[0],A9:A8
00000560   099803a6           LDNDW.D2T2    *+B6[0],B19:B18
00000564   0b141fda           MV.L2X        A5,B22
00000568   0f196e00           MPYSP.M1      A11,A6,A30
0000056c       ae3d           LDW.D2T1      *B15[17],A3
0000056e       d486           MV.L1X        B9,A6
00000570       e807           MV.L2         B16,B7
00000572       1bcf ||        MV.S2X        A7,B16
00000574       e406 ||        MV.L1         A8,A7
00000576       9e2d ||        LDW.D2T2      *B15[16],B2
00000578   02d26a64 ||        LDW.D1T1      *+A20[A19],A5
0000057c   e7000f00           .fphead       n, l, W, BU, nobr, nosat, 0111000b
00000580   03840375           STNDW.D1T1    A7:A6,*+A1[0]
00000584   037fce00 ||        MPYSP.M1      A30,A31,A6
00000588       6206           MV.L1         A4,A19
0000058a       18ce ||        MV.S1X        B17,A16
0000058c   023e42e5 ||        LDW.D2T1      *+B15[18],A4
00000590       2a47 ||        MV.L2         B20,B17
00000592       828f ||        MV.S2         B5,B20
00000594   0ab003f6           STNDW.D2T2    B21:B20,*+B12[0]
00000598   0402ae01           MPYSP.M1      A21,A0,A8
0000059c   e2880130           .fphead       n, h, W, BU, nobr, nosat, 0010100b
000005a0   08e40374 ||        STNDW.D1T1    A17:A16,*+A25[0]
000005a4   0294ce01           MPYSP.M1      A6,A5,A5
000005a8   00f002f6 ||        STW.D2T2      B1,*+B28[0]
000005ac   09e80374           STNDW.D1T1    A19:A18,*+A26[0]
000005b0   088c03f7           STNDW.D2T2    B17:B16,*+B3[0]
000005b4       cb87 ||        MV.L2         B23,B6
000005b6       ae47           MV.L2         B4,B29
000005b8   03fc03f6 ||        STNDW.D2T2    B7:B6,*+B31[0]
000005bc   e4000800           .fphead       n, l, W, BU, nobr, nosat, 0100000b
000005c0   02950219           ADDSP.L1      A8,A5,A5
000005c4   090802f7 ||        STW.D2T2      B18,*+B2[0]
000005c8   0c2416a3 ||        MV.S2X        A9,B24
000005cc       e407 ||        MV.L2         B8,B23
000005ce       8dc7           MV.L2         B19,B28
000005d0   0ba803f7 ||        STNDW.D2T2    B23:B22,*+B10[0]
000005d4   040d91a2 ||        ADD.S2X       12,A3,B8
000005d8   0491105b           ADD.L2X       8,A4,B9
000005dc   e1080080           .fphead       n, h, W, BU, nobr, nosat, 0001000b
000005e0   0eac03f7 ||        STNDW.D2T2    B29:B28,*+B11[0]
000005e4       2c4f ||        MV.S2         B0,B25
000005e6       4c67           SPLOOPD       9
000005e8   0cf803f7 ||        STNDW.D2T2    B25:B24,*+B30[0]
000005ec       1641 ||        ADD.L2X       A4,8,B4
000005ee       2de7           SPMASK        L1,L2,S1
000005f0       2646 ||^       MV.L1         A4,A9
000005f2       064e ||^       MV.S1         A4,A8
000005f4   0811105b ||^       ADD.L2X       8,A4,B16
000005f8   08905724 ||        LDNDW.D1T1    *A4++[2],A17:A16
000005fc   e3400388           .fphead       n, l, W, BU, nobr, nosat, 0011010b
00000600       2ce7           SPMASK        L1,L2
00000602       b2c7 ||^       MV.L2X        A5,B5
00000604   030e9725 ||        LDNDW.D1T1    *A3++(20),A7:A6
00000608   028d0058 ||^       ADD.L1        8,A3,A5
0000060c   0814b664           LDW.D1T1      *A5++[5],A16
00000610   00004000           NOP           3
00000614   0340ee00           MPYSP.M1      A7,A16,A6
00000618   039057a6           LDNDW.D2T2    *B4++[2],B7:B6
0000061c   e0200003           .fphead       n, l, W, BU, nobr, nosat, 0000001b
00000620   092297a7           LDNDW.D2T2    *B8++(20),B19:B18
00000624   08460e00 ||        MPYSP.M1      A16,A17,A16
00000628   091b40f0           MVD.M1        A6,A18
0000062c   00002000           NOP           2
00000630   039a0218           ADDSP.L1      A16,A6,A7
00000634   031a4e02           MPYSP.M2      B18,B6,B6
00000638   089e6e02           MPYSP.M2      B19,B7,B17
0000063c   03a49664           LDW.D1T1      *A9++[4],A7
00000640   00000000           NOP
00000644   0a1cd21a           ADDSP.L2X     B6,A7,B20
00000648   00004000           NOP           3
0000064c   02d2221b           ADDSP.L2      B17,B20,B5
00000650   0348be02 ||        MPYSP.M2X     B5,A18,B6
00000654   03141fd8           MV.L1X        B5,A6
00000658   03a05774           STNDW.D1T1    A7:A6,*A8++[2]
0000065c   09c096e6           LDW.D2T2      *B16++[4],B19
00000660   0294c21a           ADDSP.L2      B6,B5,B5
00000664       4c6e           NOP           3
00000666       4ad7           MV.D2         B5,B18
00000668   00034001           SPKERNEL      0,0
0000066c   09a457f6 ||        STNDW.D2T2    B19:B18,*B9++[2]
00000670       c1d2           MVK.S1        70,A3
00000672       a252           MVK.S1        69,A4
00000674   00010000           NOP           9
00000678   00010000           NOP           9
0000067c   e2400000           .fphead       n, l, W, BU, nobr, nosat, 0010010b
00000680   00002000           NOP           2
00000684   02508a64           LDW.D1T1      *+A20[A4],A4
00000688   01d06a64           LDW.D1T1      *+A20[A3],A3
0000068c   0fd00264           LDW.D1T1      *+A20[0],A31
00000690   0f504264           LDW.D1T1      *+A20[2],A30
00000694   026c02e6           LDW.D2T2      *+B27[0],B4
00000698   02149e00           MPYSP.M1X     A4,B5,A4
0000069c   018eee00           MPYSP.M1      A23,A3,A3
000006a0   0afd4238           SUBSP.L1      A10,A31,A21
000006a4       fe6d           LDW.D2T2      *B15[19],B6
000006a6       0c6e           NOP           1
000006a8   01906218           ADDSP.L1      A3,A4,A3
000006ac   00004000           NOP           3
000006b0   01f86e00           MPYSP.M1      A3,A30,A3
000006b4   02549e02           MPYSP.M2X     B4,A21,B4
000006b8       ef51           ADD.L2        B6,-1,B5
000006ba       ff00           ADD.L1X       B6,-1,A0
000006bc   e8400000           .fphead       n, l, W, BU, nobr, nosat, 1000010b
000006c0   018fee00           MPYSP.M1      A31,A3,A3
000006c4       fe55           STW.D2T2      B5,*B15[19]
000006c6       0ae6    [ A0]  MVK.L1        0,A5
000006c8   c296bd88    [ A0]  SET.S1        A5,21,29,A5
000006cc   00000000           NOP
000006d0   020c921a           ADDSP.L2X     B4,A3,B4
000006d4   00004000           NOP           3
000006d8   026c02f6           STW.D2T2      B4,*+B27[0]
000006dc   e0400000           .fphead       n, l, W, BU, nobr, nosat, 0000010b
000006e0   026d02e6           LDW.D2T2      *+B27[8],B4
000006e4   00006000           NOP           4
000006e8   0212be00           MPYSP.M1X     A21,B4,A4
000006ec   00002000           NOP           2
000006f0   cfff5190    [ A0]  B.S1          $C$L2 (PC-1396 = 0x0000016c)
000006f4   018c8218           ADDSP.L1      A4,A3,A3
000006f8   c23cc2e4    [ A0]  LDW.D2T1      *+B15[6],A4
000006fc   00002000           NOP           2
00000700   01ed02f5           STW.D2T1      A3,*+B27[8]
00000704   0dec805a ||        ADD.L2        4,B27,B27
00000708   01bed2e6           LDW.D2T2      *++B15[22],B3
0000070c       c677           LDDW.D2T1     *++B15[1],A13:A12
0000070e       c777           LDDW.D2T1     *++B15[1],A15:A14
00000710       d577           LDDW.D2T2     *++B15[1],B11:B10
00000712       d677           LDDW.D2T2     *++B15[1],B13:B12
00000714       6577           LDW.D2T1      *++B15[2],A10
00000716       01ef ||        BNOP.S2       B3,0
00000718       65f7           LDW.D2T1      *++B15[2],A11
0000071a       6c6e           NOP           4
0000071c   ef000400           .fphead       n, l, W, BU, nobr, nosat, 1111000b
00000720            Fx_DLY_TapeEcho3_tapmuteOpen:
00000720       e02c           LDW.D1T1      *A4[7],A2
00000722       201c           LDW.D1T1      *A4[1],A1
00000724   01803fa8           MVK.S1        0x007f,A3
00000728   0233332a           MVK.S2        0x6666,B4
0000072c   00003228           MVK.S1        0x0064,A0
00000730   00081362           B.S2X         A2
00000734   0222186a           MVKH.S2       0x44300000,B4
00000738       f582           SHL.S1        A3,0x17,A3
0000073a       0822           SET.S1        A0,8,8,A0
0000073c   e8200000           .fphead       n, l, W, BU, nobr, nosat, 1000001b
00000740       00c0           ADD.L1        A0,A1,A4
00000742       d246           MV.L1X        B4,A6
00000744   020c1fda ||        MV.L2X        A3,B4
00000748            DLY_EP3_Calc_DelayTime:
00000748       31f7           STW.D2T2      B3,*B15--[2]
0000074a       c246 ||        MV.L1         A4,A6
0000074c   0f9be266           LDW.D1T2      *+A6[31],B31
00000750   10020413           CALLP.S2      __call_stub (PC+4128 = 0x00001760),B3
00000754       014c ||        LDW.D1T1      *A6[0],A4
00000756       c627 ||        MVK.L2        6,B4
00000758       0246           MV.L1         A4,A0
0000075a       a82a    [ A0]  BNOP.S1       $C$L1 (PC+64 = 0x00000780),5
0000075c   eca08412           .fphead       n, l, W, BU, br, nosat, 1100101b
00000760   001be264           LDW.D1T1      *+A6[31],A0
00000764       014c           LDW.D1T1      *A6[0],A4
00000766       8627           MVK.L2        4,B4
00000768   00002000           NOP           2
0000076c   00001362           B.S2X         A0
00000770   01858162           ADDKPC.S2     $C$RL2 (PC+20 = 0x00000774),B3,4
00000774            $C$RL2:
00000774   00288120           BNOP.S1       $C$L2 (PC+80 = 0x000007b0),4
00000778   02114058           ADD.L1        10,A4,A4
0000077c   e0400000           .fphead       n, l, W, BU, nobr, nosat, 0000010b
00000780            $C$L1:
00000780   02180264           LDW.D1T1      *+A6[0],A4
00000784   1001fc13           CALLP.S2      __call_stub (PC+4064 = 0x00001760),B3
00000788   0f9be266 ||        LDW.D1T2      *+A6[31],B31
0000078c   009b0264           LDW.D1T1      *+A6[24],A1
00000790   0013e058           SUB.L1        A4,0x1,A0
00000794   02079e2a           MVK.S2        0x0f3c,B4
00000798       9862           EXTU.S1       A0,24,24,A4
0000079a       0c6e           NOP           1
0000079c   e8000000           .fphead       n, l, W, BU, nobr, nosat, 1000000b
000007a0   00041362           B.S2X         A1
000007a4   01828162           ADDKPC.S2     $C$RL4 (PC+8 = 0x000007a8),B3,4
000007a8            $C$RL4:
000007a8   0200322a           MVK.S2        0x0064,B4
000007ac   1001b812           CALLP.S2      __divu (PC+3520 = 0x00001560),B3
000007b0            $C$L2:
000007b0       71f7           LDW.D2T2      *++B15[2],B3
000007b2       6c6e           NOP           4
000007b4   008ca362           BNOP.S2       B3,5
000007b8            Fx_DLY_TapeEcho3_tapmuteClose:
000007b8       e02c           LDW.D1T1      *A4[7],A2
000007ba       201c           LDW.D1T1      *A4[1],A1
000007bc   ea000000           .fphead       n, l, W, BU, nobr, nosat, 1010000b
000007c0   02b3332a           MVK.S2        0x6666,B5
000007c4   02a2186a           MVKH.S2       0x44300000,B5
000007c8   00003228           MVK.S1        0x0064,A0
000007cc   00081362           B.S2X         A2
000007d0       0822           SET.S1        A0,8,8,A0
000007d2       00c0           ADD.L1        A0,A1,A4
000007d4       d2c6           MV.L1X        B5,A6
000007d6       0627           MVK.L2        0,B4
000007d8   00000000           NOP
000007dc   e6000000           .fphead       n, l, W, BU, nobr, nosat, 0110000b
000007e0            Fx_DLY_TapeEcho3_time_edit:
000007e0   10020410           CALLP.S1      __push_rts (PC+4128 = 0x00001800),A3
000007e4       200c           LDW.D1T1      *A4[1],A0
000007e6       4646           MV.L1         A4,A10
000007e8       4c6e           NOP           3
000007ea       6446           MV.L1         A0,A11
000007ec       400c ||        LDW.D1T1      *A4[2],A0
000007ee       6c6e           NOP           4
000007f0       f69b           CALLP.S2      DLY_EP3_Calc_DelayTime (PC-152 = 0x00000748),B3
000007f2       8446 ||        MV.L1         A0,A12
000007f4       1852           MVK.S1        88,A0
000007f6       0822           SET.S1        A0,8,8,A0
000007f8   05ac0078           ADD.L1        A0,A11,A11
000007fc   e7c08120           .fphead       n, l, W, BU, br, nosat, 0111110b
00000800       0586           MV.L1         A11,A0
00000802       8433           MVK.S2        164,B0
00000804       1041           ADD.L2X       B0,A0,B4
00000806       0045           STW.D2T1      A4,*B4[0]
00000808   02280fd8 ||        MV.L1         A10,A4
0000080c   1001ec13           CALLP.S2      __call_stub (PC+3936 = 0x00001760),B3
00000810   0f93c266 ||        LDW.D1T2      *+A4[30],B31
00000814   00101fda           MV.L2X        A4,B0
00000818   30594120    [!B0]  BNOP.S1       $C$L4 (PC+178 = 0x000008b2),2
0000081c   e0600008           .fphead       n, l, W, BU, nobr, nosat, 0000011b
00000820       1032           MVK.S1        48,A0
00000822       2822           SET.S1        A0,9,9,A0
00000824   06300078           ADD.L1        A0,A12,A12
00000828   02280fd8           MV.L1         A10,A4
0000082c   1001e813           CALLP.S2      __call_stub (PC+3904 = 0x00001760),B3
00000830   0f93c266 ||        LDW.D1T2      *+A4[30],B31
00000834       8226           CMPEQ.L1      4,A4,A0
00000836       a63a    [!A0]  BNOP.S1       $C$L3 (PC+48 = 0x00000850),5
00000838   02280fd8           MV.L1         A10,A4
0000083c   e4208000           .fphead       n, l, W, BU, br, nosat, 0100001b
00000840   0f93e266           LDW.D1T2      *+A4[31],B31
00000844   1001e413           CALLP.S2      __call_stub (PC+3872 = 0x00001760),B3
00000848       004c ||        LDW.D1T1      *A4[0],A4
0000084a       c627 ||        MVK.L2        6,B4
0000084c       0246           MV.L1         A4,A0
0000084e       ae6a    [ A0]  BNOP.S1       $C$L4 (PC+114 = 0x000008b2),5
00000850            $C$L3:
00000850       f79b           CALLP.S2      Fx_DLY_TapeEcho3_tapmuteClose (PC-136 = 0x000007b8),B3
00000852       8506 ||        MV.L1         A10,A4
00000854       2586           MV.L1         A11,A1
00000856       8432 ||        MVK.S1        164,A0
00000858       00c0           ADD.L1        A0,A1,A4
0000085a       000c           LDW.D1T1      *A4[0],A0
0000085c   ef808510           .fphead       n, l, W, BU, br, nosat, 1111100b
00000860       3c33           MVK.S2        185,B0
00000862       0823           SET.S2        B0,8,8,B0
00000864   0228a35a           MVK.L2        10,B4
00000868   01839028           MVK.S1        0x0720,A3
0000086c   03001572           MPYLI.M2X     B0,A0,B7:B6
00000870   01800068           MVKH.S1       0x0000,A3
00000874       4c6e           NOP           3
00000876       9346           MV.L1X        B6,A4
00000878   10018412 ||        CALLP.S2      __divi (PC+3104 = 0x00001480),B3
0000087c   e4200800           .fphead       n, l, W, BU, nobr, nosat, 0100001b
00000880   0011aca1           SHL.S1        A4,0xd,A0
00000884       8586 ||        MV.L1         A11,A4
00000886       4804           STW.D1T1      A0,*A4[10]
00000888       8606 ||        MV.L1         A12,A4
0000088a       8506           MV.L1         A10,A4
0000088c   00124274 ||        STW.D1T1      A0,*+A4[18]
00000890   00120264           LDW.D1T1      *+A4[16],A0
00000894       2586           MV.L1         A11,A1
00000896       8433           MVK.S2        164,B0
00000898       10c1           ADD.L2X       B0,A1,B4
0000089a       100d           LDW.D2T2      *B4[0],B0
0000089c   ecc00028           .fphead       n, l, W, BU, nobr, nosat, 1100110b
000008a0   00001362           B.S2X         A0
000008a4       006c           LDW.D1T1      *A4[0],A6
000008a6       4e27           MVK.L2        10,B4
000008a8   01842162           ADDKPC.S2     $C$RL15 (PC+16 = 0x000008b0),B3,1
000008ac       8041           ADD.L2        B4,B0,B4
000008ae       81c6 ||        MV.L1         A3,A4
000008b0            $C$RL15:
000008b0       a80a           BNOP.S1       $C$L5 (PC+64 = 0x000008e0),5
000008b2            $C$L4:
000008b2       8432           MVK.S1        164,A0
000008b4       2586 ||        MV.L1         A11,A1
000008b6       00c0           ADD.L1        A0,A1,A4
000008b8       000c           LDW.D1T1      *A4[0],A0
000008ba       3c33           MVK.S2        185,B0
000008bc   ef408240           .fphead       n, l, W, BU, br, nosat, 1111010b
000008c0       0823           SET.S2        B0,8,8,B0
000008c2       4e27           MVK.L2        10,B4
000008c4   00000000           NOP
000008c8   03001572           MPYLI.M2X     B0,A0,B7:B6
000008cc       6c6e           NOP           4
000008ce       9346           MV.L1X        B6,A4
000008d0   10017812 ||        CALLP.S2      __divi (PC+3008 = 0x00001480),B3
000008d4   0011aca1           SHL.S1        A4,0xd,A0
000008d8       8586 ||        MV.L1         A11,A4
000008da       4804           STW.D1T1      A0,*A4[10]
000008dc   e9200080           .fphead       n, l, W, BU, nobr, nosat, 1001001b
000008e0            $C$L5:
000008e0   1001e010           CALLP.S1      __c6xabi_pop_rts (PC+3840 = 0x000017e0),A3
000008e4            Fx_DLY_TapeEcho3_Booster_onf:
000008e4       31f7           STW.D2T2      B3,*B15--[2]
000008e6       c246 ||        MV.L1         A4,A6
000008e8   0f9be266           LDW.D1T2      *+A6[31],B31
000008ec       210c           LDW.D1T1      *A6[1],A0
000008ee       014c           LDW.D1T1      *A6[0],A4
000008f0   1001d013 ||        CALLP.S2      __call_stub (PC+3712 = 0x00001760),B3
000008f4       e627 ||        MVK.L2        7,B4
000008f6       1247           MV.L2X        A4,B0
000008f8   203d6120    [ B0]  BNOP.S1       $C$L7 (PC+122 = 0x0000095a),3
000008fc   e5400084           .fphead       n, l, W, BU, nobr, nosat, 0101010b
00000900       9092           MVK.S1        20,A1
00000902       0090           ADD.L1        A0,A1,A1
00000904       014c           LDW.D1T1      *A6[0],A4
00000906       0627           MVK.L2        0,B4
00000908   0f9be267 ||        LDW.D1T2      *+A6[31],B31
0000090c   1001cc12 ||        CALLP.S2      __call_stub (PC+3680 = 0x00001760),B3
00000910   00101fda           MV.L2X        A4,B0
00000914   201ba120    [ B0]  BNOP.S1       $C$L6 (PC+54 = 0x00000936),5
00000918   0018e264           LDW.D1T1      *+A6[7],A0
0000091c   e0600008           .fphead       n, l, W, BU, nobr, nosat, 0000011b
00000920   03333328           MVK.S1        0x6666,A6
00000924   03221868           MVKH.S1       0x44300000,A6
00000928       80c6           MV.L1         A1,A4
0000092a       0c6e           NOP           1
0000092c   00001362           B.S2X         A0
00000930   01858162           ADDKPC.S2     $C$RL24 (PC+20 = 0x00000934),B3,4
00000934            $C$RL24:
00000934       ac0a           BNOP.S1       $C$L8 (PC+96 = 0x00000980),5
00000936            $C$L6:
00000936       e10c           LDW.D1T1      *A6[7],A0
00000938   01b33328           MVK.S1        0x6666,A3
0000093c   e4808000           .fphead       n, l, W, BU, br, nosat, 0100100b
00000940   02003faa           MVK.S2        0x007f,B4
00000944   01a21868           MVKH.S1       0x44300000,A3
00000948   0212eca2           SHL.S2        B4,0x17,B4
0000094c   00001362           B.S2X         A0
00000950   01866162           ADDKPC.S2     $C$RL25 (PC+24 = 0x00000958),B3,3
00000954       c1c6           MV.L1         A3,A6
00000956       80ce ||        MV.S1         A1,A4
00000958            $C$RL25:
00000958       a80a           BNOP.S1       $C$L8 (PC+64 = 0x00000980),5
0000095a            $C$L7:
0000095a       e10c           LDW.D1T1      *A6[7],A0
0000095c   ec008400           .fphead       n, l, W, BU, br, nosat, 1100000b
00000960   01bc52e6           LDW.D2T2      *++B15[2],B3
00000964   01b33328           MVK.S1        0x6666,A3
00000968   01a21868           MVKH.S1       0x44300000,A3
0000096c   02003faa           MVK.S2        0x007f,B4
00000970   00001362           B.S2X         A0
00000974       c1c6           MV.L1         A3,A6
00000976       f603           SHL.S2        B4,0x17,B4
00000978       80c6           MV.L1         A1,A4
0000097a       2c6e           NOP           2
0000097c   ec000000           .fphead       n, l, W, BU, nobr, nosat, 1100000b
00000980            $C$L8:
00000980       71f7           LDW.D2T2      *++B15[2],B3
00000982       6c6e           NOP           4
00000984   008ca362           BNOP.S2       B3,5
00000988            Fx_DLY_TapeEcho3_onf:
00000988   1001d010           CALLP.S1      __push_rts (PC+3712 = 0x00001800),A3
0000098c       200c           LDW.D1T1      *A4[1],A0
0000098e       4646           MV.L1         A4,A10
00000990       4c6e           NOP           3
00000992       f65b           CALLP.S2      Fx_DLY_TapeEcho3_Booster_onf (PC-156 = 0x000008e4),B3
00000994       6446 ||        MV.L1         A0,A11
00000996       19d2           MVK.S1        88,A3
00000998       09a2           SET.S1        A3,8,8,A3
0000099a       8506 ||        MV.L1         A10,A4
0000099c   ef20b200           .fphead       n, l, W, BU, br, nosat, 1111001b
000009a0       0596 ||        MV.D1         A11,A0
000009a2       6000           ADD.L1        A3,A0,A0
000009a4       083c ||        LDW.D1T1      *A4[8],A3
000009a6       fbf2           MVK.S1        127,A7
000009a8   0262faaa ||        MVK.S2        0xffffc5f5,B4
000009ac   021d3beb           MVKH.S2       0x3a770000,B4
000009b0       f782 ||        SHL.S1        A7,0x17,A7
000009b2       1212           MVK.S1        16,A4
000009b4       d246           MV.L1X        B4,A6
000009b6       93c7 ||        MV.L2X        A7,B4
000009b8   1001b813           CALLP.S2      __call_stub (PC+3520 = 0x00001760),B3
000009bc   e660040a           .fphead       n, l, W, BU, nobr, nosat, 0110011b
000009c0       fdc7 ||        MV.L2X        A3,B31
000009c2       0240 ||        ADD.L1        A0,A4,A4
000009c4   02280fd8           MV.L1         A10,A4
000009c8   0f93e266           LDW.D1T2      *+A4[31],B31
000009cc   1001b413           CALLP.S2      __call_stub (PC+3488 = 0x00001760),B3
000009d0       004c ||        LDW.D1T1      *A4[0],A4
000009d2       0627 ||        MVK.L2        0,B4
000009d4   00101fda           MV.L2X        A4,B0
000009d8   2022a120    [ B0]  BNOP.S1       $C$L9 (PC+68 = 0x00000a04),5
000009dc   e2200101           .fphead       n, l, W, BU, nobr, nosat, 0010001b
000009e0       8506           MV.L1         A10,A4
000009e2       e03c           LDW.D1T1      *A4[7],A3
000009e4   03333328           MVK.S1        0x6666,A6
000009e8   02000fd8           MV.L1         A0,A4
000009ec   03221868           MVKH.S1       0x44300000,A6
000009f0       0c6e           NOP           1
000009f2       fdc7           MV.L2X        A3,B31
000009f4   1001b012 ||        CALLP.S2      __call_stub (PC+3456 = 0x00001760),B3
000009f8       dd9b           CALLP.S2      Fx_DLY_TapeEcho3_tapmuteClose (PC-552 = 0x000007b8),B3
000009fa       8506 ||        MV.L1         A10,A4
000009fc   ea209200           .fphead       n, l, W, BU, br, nosat, 1010001b
00000a00   0028a120           BNOP.S1       $C$RL35 (PC+80 = 0x00000a50),5
00000a04            $C$L9:
00000a04       8506           MV.L1         A10,A4
00000a06       e03c           LDW.D1T1      *A4[7],A3
00000a08   03333328           MVK.S1        0x6666,A6
00000a0c       93c7           MV.L2X        A7,B4
00000a0e       8046           MV.L1         A0,A4
00000a10   03221868           MVKH.S1       0x44300000,A6
00000a14   1001ac13           CALLP.S2      __call_stub (PC+3424 = 0x00001760),B3
00000a18       fdc7 ||        MV.L2X        A3,B31
00000a1a       85b2           MVK.S1        164,A3
00000a1c   e9400000           .fphead       n, l, W, BU, nobr, nosat, 1001010b
00000a20       6040           ADD.L1        A3,A0,A4
00000a22       000c           LDW.D1T1      *A4[0],A0
00000a24   02280fd8 ||        MV.L1         A10,A4
00000a28   00920264           LDW.D1T1      *+A4[16],A1
00000a2c   03100264           LDW.D1T1      *+A4[0],A6
00000a30   0203902a           MVK.S2        0x0720,B4
00000a34   0200006a           MVKH.S2       0x0000,B4
00000a38   00000000           NOP
00000a3c   e0200002           .fphead       n, l, W, BU, nobr, nosat, 0000001b
00000a40   00041362           B.S2X         A1
00000a44       9246           MV.L1X        B4,A4
00000a46       4e27           MVK.L2        10,B4
00000a48   01842162           ADDKPC.S2     $C$RL35 (PC+16 = 0x00000a50),B3,1
00000a4c   0200907a           ADD.L2X       B4,A0,B4
00000a50            $C$RL35:
00000a50            $C$L10:
00000a50   1001b410           CALLP.S1      __c6xabi_pop_rts (PC+3488 = 0x000017e0),A3
00000a54            Fx_DLY_TapeEcho3_mix_edit:
00000a54   1001b810           CALLP.S1      __push_rts (PC+3520 = 0x00001800),A3
00000a58       e246           MV.L1         A4,A7
00000a5a       218c           LDW.D1T1      *A7[1],A0
00000a5c   e8400000           .fphead       n, l, W, BU, nobr, nosat, 1000010b
00000a60       01cc           LDW.D1T1      *A7[0],A4
00000a62       6627           MVK.L2        3,B4
00000a64   0f9fe267 ||        LDW.D1T2      *+A7[31],B31
00000a68   1001a012 ||        CALLP.S2      __call_stub (PC+3328 = 0x00001760),B3
00000a6c       fbf3           MVK.S2        127,B7
00000a6e       f783           SHL.S2        B7,0x17,B7
00000a70       8372           MVK.S1        100,A6
00000a72       1a77           MVK.D2        0,B4
00000a74   0f9ea267 ||        LDW.D1T2      *+A7[21],B31
00000a78   1001a013 ||        CALLP.S2      __call_stub (PC+3328 = 0x00001760),B3
00000a7c   e3200202           .fphead       n, l, W, BU, nobr, nosat, 0011001b
00000a80       0646 ||        MV.L1         A4,A8
00000a82       93ce ||        MV.S1X        B7,A4
00000a84       a727 ||        MVK.L2        5,B6
00000a86       11f2           MVK.S1        112,A3
00000a88       09a2           SET.S1        A3,8,8,A3
00000a8a       6000           ADD.L1        A3,A0,A0
00000a8c   019ce264 ||        LDW.D1T1      *+A7[7],A3
00000a90   00b33328           MVK.S1        0x6666,A1
00000a94   00a21868           MVKH.S1       0x44300000,A1
00000a98       9247           MV.L2X        A4,B4
00000a9a       8c40           ADD.L1        A0,-4,A4
00000a9c   e8e00023           .fphead       n, l, W, BU, nobr, nosat, 1000111b
00000aa0       c0c6           MV.L1         A1,A6
00000aa2       fdc7 ||        MV.L2X        A3,B31
00000aa4   10019812 ||        CALLP.S2      __call_stub (PC+3264 = 0x00001760),B3
00000aa8       01cc           LDW.D1T1      *A7[0],A4
00000aaa       6627           MVK.L2        3,B4
00000aac   0f9fe267 ||        LDW.D1T2      *+A7[31],B31
00000ab0   10019812 ||        CALLP.S2      __call_stub (PC+3264 = 0x00001760),B3
00000ab4       e053           MVK.S2        71,B0
00000ab6       1e69           CMPGTU.L2X    B0,A4,B0
00000ab8   20348120    [ B0]  BNOP.S1       $C$L11 (PC+104 = 0x00000b08),4
00000abc   e4a00023           .fphead       n, l, W, BU, nobr, nosat, 0100101b
00000ac0   0500a359           MVK.L1        0,A10
00000ac4       b0c7 ||        MV.L2X        A1,B5
00000ac6       23cf ||        MV.S2         B7,B1
00000ac8   021c0264           LDW.D1T1      *+A7[0],A4
00000acc   10019413           CALLP.S2      __call_stub (PC+3232 = 0x00001760),B3
00000ad0   0f9fe266 ||        LDW.D1T2      *+A7[31],B31
00000ad4       db12           MVK.S1        30,A6
00000ad6       e1d2           MVK.S1        71,A3
00000ad8   10019413           CALLP.S2      __call_stub (PC+3232 = 0x00001760),B3
00000adc   e4400004           .fphead       n, l, W, BU, nobr, nosat, 0100010b
00000ae0   0f9ea267 ||        LDW.D1T2      *+A7[21],B31
00000ae4   040c80f9 ||        SUB.L1        A4,A3,A8
00000ae8       850e ||        MV.S1         A10,A4
00000aea       80c7 ||        MV.L2         B1,B4
00000aec       e1bc           LDW.D1T1      *A7[7],A3
00000aee       d2c6           MV.L1X        B5,A6
00000af0       9247           MV.L2X        A4,B4
00000af2       8046           MV.L1         A0,A4
00000af4   00000000           NOP
00000af8   000c1362           B.S2X         A3
00000afc   e3800010           .fphead       n, l, W, BU, nobr, nosat, 0011100b
00000b00   01818162           ADDKPC.S2     $C$RL48 (PC+4 = 0x00000b04),B3,4
00000b04            $C$RL48:
00000b04   000ca120           BNOP.S1       $C$RL49 (PC+24 = 0x00000b18),5
00000b08            $C$L11:
00000b08       e1bc           LDW.D1T1      *A7[7],A3
00000b0a       8046           MV.L1         A0,A4
00000b0c       83c7           MV.L2         B7,B4
00000b0e       2c6e           NOP           2
00000b10   000c1362           B.S2X         A3
00000b14   01868162           ADDKPC.S2     $C$RL49 (PC+24 = 0x00000b18),B3,4
00000b18            $C$RL49:
00000b18            $C$L12:
00000b18   10019c10           CALLP.S1      __c6xabi_pop_rts (PC+3296 = 0x000017e0),A3
00000b1c   e1800000           .fphead       n, l, W, BU, nobr, nosat, 0001100b
00000b20            Fx_DLY_TapeEcho3_RecLvl_edit:
00000b20   10019c10           CALLP.S1      __push_rts (PC+3296 = 0x00001800),A3
00000b24       b247           MV.L2X        A4,B5
00000b26       ccf7 ||        SUBAW.D2      B15,0x6,B15
00000b28       309d           LDW.D2T2      *B5[1],B1
00000b2a       00cd           LDW.D2T1      *B5[0],A4
00000b2c   10018813           CALLP.S2      __call_stub (PC+3136 = 0x00001760),B3
00000b30   0f97e2e7 ||        LDW.D2T2      *+B5[31],B31
00000b34       4627 ||        MVK.L2        2,B4
00000b36       a072           MVK.S1        101,A0
00000b38   0230212a ||        MVK.S2        0x6042,B4
00000b3c   e4c00804           .fphead       n, l, W, BU, nobr, nosat, 0100110b
00000b40   021df2eb           MVKH.S2       0x3be50000,B4
00000b44   021999a9 ||        MVK.S1        0x3333,A4
00000b48       0646 ||        MV.L1         A4,A8
00000b4a       c046           MV.L1         A0,A6
00000b4c   0f96a2e7 ||        LDW.D2T2      *+B5[21],B31
00000b50   021ff9e9 ||        MVKH.S1       0x3ff30000,A4
00000b54   10018413 ||        CALLP.S2      __call_stub (PC+3104 = 0x00001760),B3
00000b58   0304a35a ||        MVK.L2        1,B6
00000b5c   e0800020           .fphead       n, l, W, BU, nobr, nosat, 0000100b
00000b60   0f97e2e6           LDW.D2T2      *+B5[31],B31
00000b64   10018013           CALLP.S2      __call_stub (PC+3072 = 0x00001760),B3
00000b68       4646 ||        MV.L1         A4,A10
00000b6a       00cd ||        LDW.D2T1      *B5[0],A4
00000b6c       0627 ||        MVK.L2        0,B4
00000b6e       1247           MV.L2X        A4,B0
00000b70   20164120    [ B0]  BNOP.S1       $C$L13 (PC+44 = 0x00000b8c),2
00000b74       19d3           MVK.S2        88,B3
00000b76       09a3           SET.S2        B3,8,8,B3
00000b78   0084607a           ADD.L2        B3,B1,B1
00000b7c   e5800030           .fphead       n, l, W, BU, nobr, nosat, 0101100b
00000b80   01e666a8           MVK.S1        0xffffcccd,A3
00000b84   019fa668           MVKH.S1       0x3f4c0000,A3
00000b88   05286e00           MPYSP.M1      A3,A10,A10
00000b8c            $C$L13:
00000b8c       18ed           LDW.D2T2      *B5[8],B6
00000b8e       89b3           MVK.S2        44,B3
00000b90   03e2faab           MVK.S2        0xffffc5f5,B7
00000b94   018c207a ||        ADD.L2        B1,B3,B3
00000b98   039d3bea           MVKH.S2       0x3a770000,B7
00000b9c   e1000000           .fphead       n, l, W, BU, nobr, nosat, 0001000b
00000ba0       91c6           MV.L1X        B3,A4
00000ba2       ef47           MV.L2         B6,B31
00000ba4   10017813 ||        CALLP.S2      __call_stub (PC+3008 = 0x00001760),B3
00000ba8       9517 ||        MV.D2X        A10,B4
00000baa       d3c6 ||        MV.L1X        B7,A6
00000bac       00cd           LDW.D2T1      *B5[0],A4
00000bae       a627           MVK.L2        5,B4
00000bb0   0f97e2e7 ||        LDW.D2T2      *+B5[31],B31
00000bb4   10017812 ||        CALLP.S2      __call_stub (PC+3008 = 0x00001760),B3
00000bb8       fa33           MVK.S2        63,B4
00000bba       0646           MV.L1         A4,A8
00000bbc   e9a02092           .fphead       n, l, W, BU, nobr, nosat, 1001101b
00000bc0       fe72 ||        MVK.S1        255,A4
00000bc2       1e03 ||        SHL.S2        B4,0x18,B4
00000bc4   10017413           CALLP.S2      __call_stub (PC+2976 = 0x00001760),B3
00000bc8   0f96a2e7 ||        LDW.D2T2      *+B5[21],B31
00000bcc       d602 ||        SHL.S1        A4,0x16,A4
00000bce       c046 ||        MV.L1         A0,A6
00000bd0       2727 ||        MVK.L2        1,B6
00000bd2       18bd           LDW.D2T2      *B5[8],B3
00000bd4       1213           MVK.S2        16,B4
00000bd6       2241           ADD.L2        B1,B4,B4
00000bd8       6246           MV.L1         A4,A3
00000bda       d3c6           MV.L1X        B7,A6
00000bdc   ef2000c1           .fphead       n, l, W, BU, nobr, nosat, 1111001b
00000be0       edc7           MV.L2         B3,B31
00000be2       9246 ||        MV.L1X        B4,A4
00000be4   10017013 ||        CALLP.S2      __call_stub (PC+2944 = 0x00001760),B3
00000be8   020c18f2 ||        MV.D2X        A3,B4
00000bec   01b33328           MVK.S1        0x6666,A3
00000bf0   019fb368           MVKH.S1       0x3f660000,A3
00000bf4   038d4238           SUBSP.L1      A10,A3,A7
00000bf8       05a6           MVK.L1        0,A3
00000bfa       53c6           MV.L1X        B7,A2
00000bfc   e8200003           .fphead       n, l, W, BU, nobr, nosat, 1000001b
00000c00   00000000           NOP
00000c04   000cee60           CMPGTSP.S1    A7,A3,A0
00000c08       a1aa    [ A0]  BNOP.S1       $C$L14 (PC+12 = 0x00000c0c),5
00000c0a       e1c6           MV.L1         A3,A7
00000c0c            $C$L14:
00000c0c       f8f2           MVK.S1        127,A1
00000c0e       f482           SHL.S1        A1,0x17,A1
00000c10   04040fd8           MV.L1         A1,A8
00000c14   0020eea0           CMPLTSP.S1    A7,A8,A0
00000c18       a42a    [ A0]  BNOP.S1       $C$L15 (PC+32 = 0x00000c20),5
00000c1a       e406           MV.L1         A8,A7
00000c1c   e9808000           .fphead       n, l, W, BU, br, nosat, 1001100b
00000c20            $C$L15:
00000c20   01a00028           MVK.S1        0x4000,A3
00000c24   01a201e8           MVKH.S1       0x44030000,A3
00000c28   019c6e00           MPYSP.M1      A3,A7,A3
00000c2c   0200a35a           MVK.L2        0,B4
00000c30   02214b6a           MVKH.S2       0x42960000,B4
00000c34   0300a358           MVK.L1        0,A6
00000c38   0424a35a           MVK.L2        9,B8
00000c3c   038c921a           ADDSP.L2X     B4,A3,B7
00000c40   0300a35a           MVK.L2        0,B6
00000c44   0f9722e6           LDW.D2T2      *+B5[25],B31
00000c48   03606068           MVKH.S1       0xc0c00000,A6
00000c4c   023c9059           ADD.L1X       4,B15,A4
00000c50   10016413 ||        CALLP.S2      __call_stub (PC+2848 = 0x00001760),B3
00000c54       83c7 ||        MV.L2         B7,B4
00000c56       bc7d           LDW.D2T2      *B15[1],B7
00000c58       dc6d           LDW.D2T2      *B15[2],B6
00000c5a       fc3d           LDW.D2T2      *B15[3],B3
00000c5c   ec000000           .fphead       n, l, W, BU, nobr, nosat, 1100000b
00000c60   04123c2a           MVK.S2        0x2478,B8
00000c64   041fd0ea           MVKH.S2       0x3fa10000,B8
00000c68   021d0e02           MPYSP.M2      B8,B7,B4
00000c6c   03190e02           MPYSP.M2      B8,B6,B6
00000c70   038d0e03           MPYSP.M2      B8,B3,B7
00000c74       f8bd ||        LDW.D2T2      *B5[15],B3
00000c76       1133           MVK.S2        48,B2
00000c78       2121           ADD.L2        B1,B2,B2
00000c7a       dc65           STW.D2T2      B6,*B15[2]
00000c7c   ec000000           .fphead       n, l, W, BU, nobr, nosat, 1100000b
00000c80   023c805b           ADD.L2        4,B15,B4
00000c84       bc45 ||        STW.D2T2      B4,*B15[1]
00000c86       edc7           MV.L2         B3,B31
00000c88   10015c13 ||        CALLP.S2      __call_stub (PC+2784 = 0x00001760),B3
00000c8c       fc75 ||        STW.D2T2      B7,*B15[3]
00000c8e       9312 ||        MVK.S1        20,A6
00000c90   02081fd8 ||        MV.L1X        B2,A4
00000c94   0240002a           MVK.S2        0xffff8000,B4
00000c98   0262046a           MVKH.S2       0xc4080000,B4
00000c9c   e14000c8           .fphead       n, l, W, BU, nobr, nosat, 0001010b
00000ca0   021c9e02           MPYSP.M2X     B4,A7,B4
00000ca4   0320002a           MVK.S2        0x4000,B6
00000ca8   0f9722e6           LDW.D2T2      *+B5[25],B31
00000cac   03226eeb           MVKH.S2       0x44dd0000,B6
00000cb0   0400a35a ||        MVK.L2        0,B8
00000cb4   10015813           CALLP.S2      __call_stub (PC+2752 = 0x00001760),B3
00000cb8   0210c21b ||        ADDSP.L2      B6,B4,B4
00000cbc   03000043 ||        MVK.D2        0,B6
00000cc0       04c6 ||        MV.L1         A1,A8
00000cc2       1b76 ||        MVK.D1        0,A6
00000cc4   023c91a0 ||        ADD.S1X       4,B15,A4
00000cc8       f8ed           LDW.D2T2      *B5[15],B6
00000cca       81d3           MVK.S2        68,B3
00000ccc   018c207a           ADD.L2        B1,B3,B3
00000cd0   023c805a           ADD.L2        4,B15,B4
00000cd4       9312           MVK.S1        20,A6
00000cd6       91c6           MV.L1X        B3,A4
00000cd8   10015413 ||        CALLP.S2      __call_stub (PC+2720 = 0x00001760),B3
00000cdc   e4a00803           .fphead       n, l, W, BU, nobr, nosat, 0100101b
00000ce0   0f980fda ||        MV.L2         B6,B31
00000ce4   01cccd28           MVK.S1        0xffff999a,A3
00000ce8   01df4ce9           MVKH.S1       0xbe990000,A3
00000cec   031502e6 ||        LDW.D2T2      *+B5[8],B6
00000cf0   019c6e00           MPYSP.M1      A3,A7,A3
00000cf4       0433           MVK.S2        160,B0
00000cf6       2001           ADD.L2        B1,B0,B0
00000cf8       c146           MV.L1         A2,A6
00000cfa       036f           BNOP.S2       B6,0
00000cfc   ec002000           .fphead       n, l, W, BU, nobr, nosat, 1100000b
00000d00   018c2218 ||        ADDSP.L1      A1,A3,A3
00000d04   02001fd8           MV.L1X        B0,A4
00000d08   01844162           ADDKPC.S2     $C$RL69 (PC+16 = 0x00000d10),B3,2
00000d0c   020c1fda           MV.L2X        A3,B4
00000d10            $C$RL69:
00000d10   10015c11           CALLP.S1      __c6xabi_pop_rts (PC+2784 = 0x000017e0),A3
00000d14   07800c52 ||        ADDK.S2       24,B15
00000d18            Fx_DLY_TapeEcho3_init:
00000d18   10016010           CALLP.S1      __push_rts (PC+2816 = 0x00001800),A3
00000d1c   00100fd8           MV.L1         A4,A0
00000d20       4646           MV.L1         A4,A10
00000d22       a256 ||        MV.D1         A4,A5
00000d24       0e12 ||        MVK.S1        136,A4
00000d26       0240           ADD.L1        A0,A4,A4
00000d28       001c           LDW.D1T1      *A4[0],A1
00000d2a       8506           MV.L1         A10,A4
00000d2c   02034c2a           MVK.S2        0x0698,B4
00000d30       400c           LDW.D1T1      *A4[2],A0
00000d32       0312           MVK.S1        0,A6
00000d34   0240006a ||        MVKH.S2       0x80000000,B4
00000d38   10014813           CALLP.S2      __call_stub (PC+2624 = 0x00001760),B3
00000d3c   e2e00203           .fphead       n, l, W, BU, nobr, nosat, 0010111b
00000d40       20cc ||        LDW.D1T1      *A5[1],A4
00000d42       fcc7 ||        MV.L2X        A1,B31
00000d44       2b22 ||        SET.S1        A6,9,9,A6
00000d46       2506           MV.L1         A10,A1
00000d48       8c13           MVK.S2        140,B0
00000d4a       10c1           ADD.L2X       B0,A1,B4
00000d4c       100d           LDW.D2T2      *B4[0],B0
00000d4e       9b72           MVK.S1        124,A6
00000d50       0627           MVK.L2        0,B4
00000d52       2b22           SET.S1        A6,9,9,A6
00000d54       8046           MV.L1         A0,A4
00000d56       ec47           MV.L2         B0,B31
00000d58   10014412 ||        CALLP.S2      __call_stub (PC+2592 = 0x00001760),B3
00000d5c   e7e00803           .fphead       n, l, W, BU, nobr, nosat, 0111111b
00000d60   10014c13           CALLP.S2      Fx_DLY_TapeEcho3_fb_edit (PC+2656 = 0x000017c0),B3
00000d64   02280fd8 ||        MV.L1         A10,A4
00000d68   1fff9e93           CALLP.S2      Fx_DLY_TapeEcho3_mix_edit (PC-780 = 0x00000a54),B3
00000d6c   02280fd8 ||        MV.L1         A10,A4
00000d70   1fff5013           CALLP.S2      Fx_DLY_TapeEcho3_time_edit (PC-1408 = 0x000007e0),B3
00000d74   02280fd8 ||        MV.L1         A10,A4
00000d78   10015010           CALLP.S1      __c6xabi_pop_rts (PC+2688 = 0x000017e0),A3
00000d7c   00000000           NOP
00000d80            SUB_TapeEcho3:
00000d80   0ea01fda           MV.L2X        A8,B29
00000d84   02f5a2e6           LDW.D2T2      *+B29[13],B5
00000d88   0a1842e4           LDW.D2T1      *+B6[2],A20
00000d8c   0480122a           MVK.S2        0x0024,B9
00000d90   0400102b           MVK.S2        0x0020,B8
00000d94   0e1822e7 ||        LDW.D2T2      *+B6[1],B28
00000d98       6e47 ||        MV.L2         B4,B27
00000d9a       a3b3           MVK.S2        37,B7
00000d9c   e8002000           .fphead       n, l, W, BU, nobr, nosat, 1000000b
00000da0   0bec62e6 ||        LDW.D2T2      *+B27[3],B23
00000da4   041402e5           LDW.D2T1      *+B5[0],A8
00000da8   080011ab ||        MVK.S2        0x0023,B16
00000dac   0d00a35b ||        MVK.L2        0,B26
00000db0       ab46 ||        MV.L1         A6,A21
00000db2       0627           MVK.L2        0,B4
00000db4   0a6c82e7 ||        LDW.D2T2      *+B27[4],B20
00000db8   02d64265 ||        LDW.D1T1      *+A21[18],A5
00000dbc   e2000200           .fphead       n, l, W, BU, nobr, nosat, 0010000b
00000dc0   0d60006a ||        MVKH.S2       0xc0000000,B26
00000dc4   09562267           LDW.D1T2      *+A21[17],B18
00000dc8   0b1802e5 ||        LDW.D2T1      *+B6[0],A22
00000dcc   0c93de8b ||        SET.S2        B4,30,30,B25
00000dd0       6246 ||        MV.L1         A4,A3
00000dd2       8712           MVK.S1        132,A6
00000dd4   0ad60267 ||        LDW.D1T2      *+A21[16],B21
00000dd8   07ffec53 ||        ADDK.S2       -40,B15
00000ddc   e2000200           .fphead       n, l, W, BU, nobr, nosat, 0010000b
00000de0   04f5c2e5 ||        LDW.D2T1      *+B29[14],A9
00000de4       24a6 ||        MVK.L1        1,A1
00000de6       c1f0           ADD.L1        A6,A3,A7
00000de8   10004001 ||        DINT
00000dec   08bd005b ||        ADD.L2        8,B15,B17
00000df0   030e3ec1 ||        ADDAD.D1      A3,0x11,A6
00000df4       e012 ||        MVK.S1        7,A0
00000df6       1e57 ||        MV.D2X        A4,B24
00000df8            $C$L2:
00000df8   91a00274    [!A1]  STW.D1T1      A3,*+A8[0]
00000dfc   e4400408           .fphead       n, l, W, BU, nobr, nosat, 0100010b
00000e00   02e20ae6           LDW.D2T2      *+B24[B16],B5
00000e04   02612ae6           LDW.D2T2      *+B24[B9],B4
00000e08   026382e6           LDW.D2T2      *+B24[28],B4
00000e0c   0263c2e6           LDW.D2T2      *+B24[30],B4
00000e10   0263a2e6           LDW.D2T2      *+B24[29],B4
00000e14   026362e6           LDW.D2T2      *+B24[27],B4
00000e18   09d00fdb           MV.L2         B20,B19
00000e1c   9a5081e2 || [!A1]  ADD.S2        B4,B20,B20
00000e20   0b5c06a3           MV.S2         B23,B22
00000e24   1312a673 ||        SMPY32.M2     B21,B4,B6
00000e28   9b96e843 || [!A1]  ADD.D2        B5,B23,B23
00000e2c   02d0035a ||        ABS.L2        B20,B5
00000e30   12924673           SMPY32.M2     B18,B4,B5
00000e34   035c035b ||        ABS.L2        B23,B6
00000e38   02e8a822 ||        SADD.S2       B5,B26,B5
00000e3c   02e8c27b           SADD.L2       B6,B26,B5
00000e40   1210a672 ||        SMPY32.M2     B5,B4,B4
00000e44   02610ae7           LDW.D2T2      *+B24[B8],B4
00000e48   1210a672 ||        SMPY32.M2     B5,B4,B4
00000e4c   02e3e2e6           LDW.D2T2      *+B24[31],B5
00000e50       0c6e           NOP           1
00000e52       01bc           LDW.D1T1      *A7[0],A3
00000e54   026142e7 ||        LDW.D2T2      *+B24[10],B4
00000e58   9914807a || [!A1]  ADD.L2        B4,B5,B18
00000e5c   e2000200           .fphead       n, l, W, BU, nobr, nosat, 0010000b
00000e60   01980265           LDW.D1T1      *+A6[0],A3
00000e64   9a9881e3 || [!A1]  ADD.S2        B4,B6,B21
00000e68   02e6427a ||        SADD.L2       B18,B25,B5
00000e6c   0266a27b           SADD.L2       B21,B25,B4
00000e70   1210a672 ||        SMPY32.M2     B5,B4,B4
00000e74   02e0eae7           LDW.D2T2      *+B24[B7],B5
00000e78   12948672 ||        SMPY32.M2     B4,B5,B5
00000e7c   00000000           NOP
00000e80   12107670           SMPY32.M1X    A3,B4,A4
00000e84   118ca670           SMPY32.M1     A5,A3,A3
00000e88   0210a27a           SADD.L2       B5,B4,B4
00000e8c   c07bd021    [ A0]  BDEC.S1       $C$L2 (PC-136 = 0x00000df8),A0
00000e90   12148672 ||        SMPY32.M2     B4,B5,B4
00000e94   01a43664           LDW.D1T1      *A9++[1],A3
00000e98   928c8078    [!A1]  ADD.L1        A4,A3,A5
00000e9c   00000000           NOP
00000ea0   0214927a           SADD.L2X      B4,A5,B4
00000ea4   8087e059    [ A1]  SUB.L1        A1,0x1,A1
00000ea8   924436f6 || [!A1]  STW.D2T2      B4,*B17++[1]
00000eac       0ca7           MVK.L2        8,B1
00000eae       05a6 ||        MVK.L1        0,A3
00000eb0   0e801429 ||        MVK.S1        0x0028,A29
00000eb4   0980002b ||        MVK.S2        0x0000,B19
00000eb8   01a00275 ||        STW.D1T1      A3,*+A8[0]
00000ebc   e10000c0           .fphead       n, l, W, BU, nobr, nosat, 0001000b
00000ec0       1a77 ||        MVK.D2        0,B4
00000ec2       0727           MVK.L2        0,B6
00000ec4   0e00a359 ||        MVK.L1        0,A28
00000ec8   0c0f9d89 ||        SET.S1        A3,28,29,A24
00000ecc   0212fd8b ||        SET.S2        B4,23,29,B4
00000ed0   02620ae7 ||        LDW.D2T2      *+B24[B16],B4
00000ed4   026018f0 ||        MV.D1X        B24,A4
00000ed8   0d8013a9           MVK.S1        0x0027,A27
00000edc   e0200002           .fphead       n, l, W, BU, nobr, nosat, 0000001b
00000ee0   04bd005b ||        ADD.L2        8,B15,B9
00000ee4   02e12ae6 ||        LDW.D2T2      *+B24[B9],B5
00000ee8   0b901fd9           MV.L1X        B4,A23
00000eec   086382e6 ||        LDW.D2T2      *+B24[28],B16
00000ef0   0863c2e6           LDW.D2T2      *+B24[30],B16
00000ef4   0963a2e6           LDW.D2T2      *+B24[29],B18
00000ef8   086362e6           LDW.D2T2      *+B24[27],B16
00000efc   02d00fdb           MV.L2         B20,B5
00000f00   0a50a1e2 ||        ADD.S2        B5,B20,B20
00000f04   0b5c06a3           MV.S2         B23,B22
00000f08   1242a673 ||        SMPY32.M2     B21,B16,B4
00000f0c   0b92e843 ||        ADD.D2        B4,B23,B23
00000f10   0250035a ||        ABS.L2        B20,B4
00000f14   14424673           SMPY32.M2     B18,B16,B8
00000f18   025c035b ||        ABS.L2        B23,B4
00000f1c   08688822 ||        SADD.S2       B4,B26,B16
00000f20   0268827b           SADD.L2       B4,B26,B4
00000f24   184a0672 ||        SMPY32.M2     B16,B18,B16
00000f28   08610ae7           LDW.D2T2      *+B24[B8],B16
00000f2c   14408672 ||        SMPY32.M2     B4,B16,B8
00000f30   0463e2e6           LDW.D2T2      *+B24[31],B8
00000f34       0c6e           NOP           1
00000f36       01bc           LDW.D1T1      *A7[0],A3
00000f38   026142e7 ||        LDW.D2T2      *+B24[10],B4
00000f3c   e4000800           .fphead       n, l, W, BU, nobr, nosat, 0100000b
00000f40   0922007a ||        ADD.L2        B16,B8,B18
00000f44   01980265           LDW.D1T1      *+A6[0],A3
00000f48   021101e3 ||        ADD.S2        B8,B4,B4
00000f4c   0466427a ||        SADD.L2       B18,B25,B8
00000f50   0864827b           SADD.L2       B4,B25,B16
00000f54   14410672 ||        SMPY32.M2     B8,B16,B8
00000f58   03cebd8b           SET.S2        B19,21,29,B7
00000f5c   03101fd9 ||        MV.L1X        B4,A6
00000f60   0260eae7 ||        LDW.D2T2      *+B24[B7],B4
00000f64   12220672 ||        SMPY32.M2     B16,B8,B4
00000f68   00000000           NOP
00000f6c   11907670           SMPY32.M1X    A3,B4,A3
00000f70   128ca670           SMPY32.M1     A5,A3,A5
00000f74   0420827a           SADD.L2       B4,B8,B8
00000f78   12110672           SMPY32.M2     B8,B4,B4
00000f7c   10006000           RINT
00000f80       62d0           ADD.L1        A3,A5,A5
00000f82       0c6e           NOP           1
00000f84   0214927a           SADD.L2X      B4,A5,B4
00000f88   024436f6           STW.D2T2      B4,*B17++[1]
00000f8c   02d5e276           STW.D1T2      B5,*+A21[15]
00000f90   0b55c276           STW.D1T2      B22,*+A21[14]
00000f94   09d52264           LDW.D1T1      *+A21[9],A19
00000f98   0d554264           LDW.D1T1      *+A21[10],A26
00000f9c   e0200000           .fphead       n, l, W, BU, nobr, nosat, 0000001b
00000fa0   0cd46266           LDW.D1T2      *+A21[3],B25
00000fa4   09540264           LDW.D1T1      *+A21[0],A18
00000fa8   01542264           LDW.D1T1      *+A21[1],A2
00000fac   0c54e266           LDW.D1T2      *+A21[7],B24
00000fb0   09562277           STW.D1T2      B18,*+A21[17]
00000fb4   04ec42e4 ||        LDW.D2T1      *+B27[2],A9
00000fb8   08548264           LDW.D1T1      *+A21[4],A16
00000fbc   08d4c264           LDW.D1T1      *+A21[6],A17
00000fc0   027562e7           LDW.D2T2      *+B29[11],B4
00000fc4   0cd50264 ||        LDW.D1T1      *+A21[8],A25
00000fc8   08f582e7           LDW.D2T2      *+B29[12],B17
00000fcc   03560274 ||        STW.D1T1      A6,*+A21[16]
00000fd0   0bec62f7           STW.D2T2      B23,*+B27[3]
00000fd4   00d4a264 ||        LDW.D1T1      *+A21[5],A1
00000fd8   0a6c82f7           STW.D2T2      B20,*+B27[4]
00000fdc   0f556264 ||        LDW.D1T1      *+A21[11],A30
00000fe0   01d44265           LDW.D1T1      *+A21[2],A3
00000fe4   0af4a2e6 ||        LDW.D2T2      *+B29[5],B21
00000fe8   099002e7           LDW.D2T2      *+B4[0],B19
00000fec   02d64274 ||        STW.D1T1      A5,*+A21[18]
00000ff0            $C$L4:
00000ff0   024436e6           LDW.D2T2      *B17++[1],B4
00000ff4   00006000           NOP           4
00000ff8   024c02f6           STW.D2T2      B4,*+B19[0]
00000ffc   025402e6           LDW.D2T2      *+B21[0],B4
00001000   02d502e6           LDW.D2T2      *+B21[8],B5
00001004       685c           LDW.D1T1      *A4[11],A5
00001006       806c           LDW.D1T1      *A4[4],A6
00001008   0411a264           LDW.D1T1      *+A4[13],A8
0000100c   0f93aa64           LDW.D1T1      *+A4[A29],A31
00001010   0210a21a           ADDSP.L2      B5,B4,B4
00001014   03952e00           MPYSP.M1      A9,A5,A7
00001018   0073ff88           SET.S1        A28,31,31,A0
0000101c   e0400000           .fphead       n, l, W, BU, nobr, nosat, 0000010b
00001020   02918264           LDW.D1T1      *+A4[12],A5
00001024   021885b2           MPYSPDP.M2    B4,B7:B6,B5:B4
00001028   04a24e00           MPYSP.M1      A18,A8,A9
0000102c   0fa436e6           LDW.D2T2      *B9++[1],B31
00001030       ec91           ADD.L2        B1,-1,B1
00001032       4c6e           NOP           3
00001034   0814813a           DPSP.L2       B5:B4,B16
00001038   00004000           NOP           3
0000103c   e2000000           .fphead       n, l, W, BU, nobr, nosat, 0010000b
00001040   021a1e02           MPYSP.M2X     B16,A6,B4
00001044   0303edf8           XOR.L1        A31,A0,A6
00001048   00002000           NOP           2
0000104c   021c921a           ADDSP.L2X     B4,A7,B4
00001050   00004000           NOP           3
00001054   007c9e62           CMPGTSP.S2X   B4,A31,B0
00001058   0010de61           CMPGTSP.S1X   A6,B4,A0
0000105c   247c1fda || [ B0]  MV.L2X        A31,B8
00001060   d3101fd8    [!A0]  MV.L1X        B4,A6
00001064   0011c264           LDW.D1T1      *+A4[14],A0
00001068   34181fda    [!B0]  MV.L2X        A6,B8
0000106c   02151e02           MPYSP.M2X     B8,A5,B4
00001070   0f91e264           LDW.D1T1      *+A4[15],A31
00001074   03120264           LDW.D1T1      *+A4[16],A6
00001078   04004e00           MPYSP.M1      A2,A0,A8
0000107c   0224921a           ADDSP.L2X     B4,A9,B4
00001080   01124264           LDW.D1T1      *+A4[18],A2
00001084   03fc6e00           MPYSP.M1      A3,A31,A7
00001088   029b3e02           MPYSP.M2X     B25,A6,B5
0000108c   0220921a           ADDSP.L2X     B4,A8,B4
00001090   00122264           LDW.D1T1      *+A4[17],A0
00001094   04926264           LDW.D1T1      *+A4[19],A9
00001098   03128264           LDW.D1T1      *+A4[20],A6
0000109c   021c921a           ADDSP.L2X     B4,A7,B4
000010a0   038a0e00           MPYSP.M1      A16,A2,A7
000010a4   0112a264           LDW.D1T1      *+A4[21],A2
000010a8   0fa42e00           MPYSP.M1      A1,A9,A31
000010ac   0910a21a           ADDSP.L2      B5,B4,B18
000010b0   04912264           LDW.D1T1      *+A4[9],A9
000010b4   029a2e00           MPYSP.M1      A17,A6,A5
000010b8   028b1e02           MPYSP.M2X     B24,A2,B5
000010bc   02025e02           MPYSP.M2X     B18,A0,B4
000010c0   0c8c1fda           MV.L2X        A3,B25
000010c4   002488d8           CMPGT.L1      4,A9,A0
000010c8   0c441fda           MV.L2X        A17,B24
000010cc   021c921a           ADDSP.L2X     B4,A7,B4
000010d0       7906           MV.L1X        B18,A3
000010d2       2c6e           NOP           2
000010d4   027c921a           ADDSP.L2X     B4,A31,B4
000010d8   00006000           NOP           4
000010dc   e2000000           .fphead       n, l, W, BU, nobr, nosat, 0010000b
000010e0   0390b218           ADDSP.L1X     A5,B4,A7
000010e4   02a78058           SUB.L1        A9,0x4,A5
000010e8   d2912274    [!A0]  STW.D1T1      A5,*+A4[9]
000010ec   c29681e0    [ A0]  ADD.S1        A20,A5,A5
000010f0   0414f218           ADDSP.L1X     A7,B5,A8
000010f4   c2912274    [ A0]  STW.D1T1      A5,*+A4[9]
000010f8   0296c078           ADD.L1        A22,A5,A5
000010fc   00000000           NOP
00001100   04140274           STW.D1T1      A8,*+A5[0]
00001104   0390e264           LDW.D1T1      *+A4[7],A7
00001108   04910264           LDW.D1T1      *+A4[8],A9
0000110c   00906264           LDW.D1T1      *+A4[3],A1
00001110   0f92e264           LDW.D1T1      *+A4[23],A31
00001114   0312c264           LDW.D1T1      *+A4[22],A6
00001118   03fcfde0           SHR.S1X       B31,A7,A7
0000111c   0294fc40           ADDAW.D1      A5,A7,A5
00001120   04fd3ce0           SHL.S1X       B31,A9,A9
00001124   001799fa           CMPGTU.L2X    B28,A5,B0
00001128   32d0a0f8    [!B0]  SUB.L1        A5,A20,A5
0000112c       86f0           ADD.L1        A5,4,A7
0000112e       00dc           LDW.D1T1      *A5[0],A5
00001130   001f99fa           CMPGTU.L2X    B28,A7,B0
00001134   33d0e0f8    [!B0]  SUB.L1        A7,A20,A7
00001138   049c0265           LDW.D1T1      *+A7[0],A9
0000113c   e1000000           .fphead       n, l, W, BU, nobr, nosat, 0001000b
00001140   03a429a0 ||        SHRU.S1       A9,0x1,A7
00001144   039c0958           INTSP.L1      A7,A7
00001148   01130264           LDW.D1T1      *+A4[24],A2
0000114c   007f2e00           MPYSP.M1      A25,A31,A0
00001150   0f5402e6           LDW.D2T2      *+B21[0],B30
00001154   04952239           SUBSP.L1      A9,A5,A9
00001158   039f0e00 ||        MPYSP.M1      A24,A7,A7
0000115c   08a00fd8           MV.L1         A8,A17
00001160   0f8a6e00           MPYSP.M1      A19,A2,A31
00001164   0110c264           LDW.D1T1      *+A4[6],A2
00001168   04a4ee00           MPYSP.M1      A7,A9,A9
0000116c   00004000           NOP           3
00001170   04a4a218           ADDSP.L1      A5,A9,A9
00001174   00004000           NOP           3
00001178   04852e00           MPYSP.M1      A9,A1,A9
0000117c   00932264           LDW.D1T1      *+A4[25],A1
00001180   00002000           NOP           2
00001184   02992e00           MPYSP.M1      A9,A6,A5
00001188   03134264           LDW.D1T1      *+A4[26],A6
0000118c   00002000           NOP           2
00001190   09940218           ADDSP.L1      A0,A5,A19
00001194   0010a264           LDW.D1T1      *+A4[5],A0
00001198   029bce00           MPYSP.M1      A30,A6,A5
0000119c   03136a64           LDW.D1T1      *+A4[A27],A6
000011a0   03cfe219           ADDSP.L1      A31,A19,A7
000011a4   09874e00 ||        MPYSP.M1      A26,A1,A19
000011a8       001c           LDW.D1T1      *A4[0],A1
000011aa       2c6e           NOP           2
000011ac   039e6218           ADDSP.L1      A19,A7,A7
000011b0   00004000           NOP           3
000011b4   0f9ca218           ADDSP.L1      A5,A7,A31
000011b8   02c05e00           MPYSP.M1X     A2,B16,A5
000011bc   e0800000           .fphead       n, l, W, BU, nobr, nosat, 0000100b
000011c0   0f06e238           SUBSP.L1      A23,A1,A30
000011c4   014806a0           MV.S1         A18,A2
000011c8   0383ee00           MPYSP.M1      A31,A0,A7
000011cc       540e           MV.S1X        B8,A18
000011ce       2c6e           NOP           2
000011d0   039ca218           ADDSP.L1      A5,A7,A7
000011d4   02fbde02           MPYSP.M2X     B30,A30,B5
000011d8   00002000           NOP           2
000011dc   e1080000           .fphead       n, h, W, BU, nobr, nosat, 0001000b
000011e0   0984ee00           MPYSP.M1      A7,A1,A19
000011e4       2046           MV.L1         A16,A1
000011e6       1946           MV.L1X        B18,A16
000011e8   00002000           NOP           2
000011ec   024cb21a           ADDSP.L2X     B5,A19,B4
000011f0   00004000           NOP           3
000011f4   02189e02           MPYSP.M2X     B4,A6,B4
000011f8   00004000           NOP           3
000011fc   e0480000           .fphead       n, h, W, BU, nobr, nosat, 0000010b
00001200   025402f6           STW.D2T2      B4,*+B21[0]
00001204   025502e6           LDW.D2T2      *+B21[8],B4
00001208   0293de00           MPYSP.M1X     A30,B4,A5
0000120c   0f680fd8           MV.L1         A26,A30
00001210   0d7c0fd8           MV.L1         A31,A26
00001214   00000000           NOP
00001218   02cca218           ADDSP.L1      A5,A19,A5
0000121c   e0100000           .fphead       p, l, W, BU, nobr, nosat, 0000000b
00001220   09e40fd8           MV.L1         A25,A19
00001224   0ca40fd8           MV.L1         A9,A25
00001228   4fffba10    [ B1]  B.S1          $C$L4 (PC-560 = 0x00000ff0)
0000122c   0294ce00           MPYSP.M1      A6,A5,A5
00001230       4c6e           NOP           3
00001232       86d1           ADD.L2        B21,4,B21
00001234   02d502f4 ||        STW.D2T1      A5,*+B21[8]
00001238   0cd46277           STW.D1T2      B25,*+A21[3]
0000123c   e2080200           .fphead       n, h, W, BU, nobr, nosat, 0010000b
00001240   07801452 ||        ADDK.S2       40,B15
00001244   0c54e276           STW.D1T2      B24,*+A21[7]
00001248   01d44274           STW.D1T1      A3,*+A21[2]
0000124c   01542274           STW.D1T1      A2,*+A21[1]
00001250   09540274           STW.D1T1      A18,*+A21[0]
00001254   04ec42f4           STW.D2T1      A9,*+B27[2]
00001258   00d4a274           STW.D1T1      A1,*+A21[5]
0000125c   08548275           STW.D1T1      A16,*+A21[4]
00001260   000c0362 ||        B.S2          B3
00001264   0f556274           STW.D1T1      A30,*+A21[11]
00001268   0d554274           STW.D1T1      A26,*+A21[10]
0000126c   09d52274           STW.D1T1      A19,*+A21[9]
00001270   0cd50274           STW.D1T1      A25,*+A21[8]
00001274   08d4c275           STW.D1T1      A17,*+A21[6]
00001278   086c02f6 ||        STW.D2T2      B16,*+B27[0]
0000127c   00000000           NOP
00001280            GetString_offset_10:
00001280       4da6           MVK.L1        10,A3
00001282       6230           ADD.L1        A3,A4,A3
00001284   000d49d8           CMPGTU.L1     0xa,A3,A0
00001288   c0798120    [ A0]  BNOP.S1       $C$L4 (PC+242 = 0x00001372),4
0000128c       a247           MV.L2         B4,B5
0000128e       15c6 ||        MV.L1X        B3,A8
00001290       8072           MVK.S1        100,A0
00001292       6c48           CMPLTU.L1     A3,A0,A0
00001294       d92a    [ A0]  BNOP.S1       $C$L3 (PC+200 = 0x00001348),5
00001296       b872           MVK.S1        125,A0
00001298       6402           SHL.S1        A0,0x3,A0
0000129a       6c48           CMPLTU.L1     A3,A0,A0
0000129c   ef208040           .fphead       n, l, W, BU, br, nosat, 1111001b
000012a0       acea    [ A0]  BNOP.S1       $C$L2 (PC+102 = 0x00001306),5
000012a2       ba73           MVK.S2        125,B4
000012a4       6603           SHL.S2        B4,0x3,B4
000012a6       81c6           MV.L1         A3,A4
000012a8   10005812 ||        CALLP.S2      __divu (PC+704 = 0x00001560),B3
000012ac       1032           MVK.S1        48,A0
000012ae       ba73 ||        MVK.S2        125,B4
000012b0       8000           ADD.L1        A4,A0,A0
000012b2       6603 ||        SHL.S2        B4,0x3,B4
000012b4   10008413           CALLP.S2      __c6xabi_remu (PC+1056 = 0x000016c0),B3
000012b8       0285 ||        STB.D2T1      A0,*B5[0]
000012ba       81c6 ||        MV.L1         A3,A4
000012bc   eb609148           .fphead       n, l, W, BU, br, nosat, 1011011b
000012c0   0200322a           MVK.S2        0x0064,B4
000012c4   10005412           CALLP.S2      __divu (PC+672 = 0x00001560),B3
000012c8       1247           MV.L2X        A4,B0
000012ca       8273           MVK.S2        100,B4
000012cc   0000dec2 ||        ADDAD.D2      B0,0x6,B0
000012d0   10008013           CALLP.S2      __c6xabi_remu (PC+1024 = 0x000016c0),B3
000012d4       3285 ||        STB.D2T2      B0,*B5[1]
000012d6       81c6 ||        MV.L1         A3,A4
000012d8   10005413           CALLP.S2      __divu (PC+672 = 0x00001560),B3
000012dc   e4800420           .fphead       n, l, W, BU, nobr, nosat, 0100100b
000012e0       4e27 ||        MVK.L2        10,B4
000012e2       1032           MVK.S1        48,A0
000012e4       8000           ADD.L1        A4,A0,A0
000012e6       4285           STB.D2T1      A0,*B5[2]
000012e8   10007c13 ||        CALLP.S2      __c6xabi_remu (PC+992 = 0x000016c0),B3
000012ec       81c6 ||        MV.L1         A3,A4
000012ee       4e27 ||        MVK.L2        10,B4
000012f0       1247           MV.L2X        A4,B0
000012f2       04a7           MVK.L2        0,B1
000012f4   0000dec2 ||        ADDAD.D2      B0,0x6,B0
000012f8   009482b6           STB.D2T2      B1,*+B5[4]
000012fc   e3600248           .fphead       n, l, W, BU, nobr, nosat, 0011011b
00001300            $C$L1:
00001300   00a09362           BNOP.S2X      A8,4
00001304       7285           STB.D2T2      B0,*B5[3]
00001306            $C$L2:
00001306       8273           MVK.S2        100,B4
00001308   10004c13           CALLP.S2      __divu (PC+608 = 0x00001560),B3
0000130c       81c6 ||        MV.L1         A3,A4
0000130e       8072           MVK.S1        100,A0
00001310   00101fda ||        MV.L2X        A4,B0
00001314   0000dec2           ADDAD.D2      B0,0x6,B0
00001318   10007813           CALLP.S2      __c6xabi_remu (PC+960 = 0x000016c0),B3
0000131c   e1400080           .fphead       n, l, W, BU, nobr, nosat, 0001010b
00001320       9047 ||        MV.L2X        A0,B4
00001322       1285 ||        STB.D2T2      B0,*B5[0]
00001324       81c6 ||        MV.L1         A3,A4
00001326       4e27           MVK.L2        10,B4
00001328   10004812 ||        CALLP.S2      __divu (PC+576 = 0x00001560),B3
0000132c       1032           MVK.S1        48,A0
0000132e       8000           ADD.L1        A4,A0,A0
00001330   10007413           CALLP.S2      __c6xabi_remu (PC+928 = 0x000016c0),B3
00001334       2285 ||        STB.D2T1      A0,*B5[1]
00001336       81c6 ||        MV.L1         A3,A4
00001338       4e27 ||        MVK.L2        10,B4
0000133a       5c0a           BNOP.S1       $C$L1 (PC-32 = 0x00001300),2
0000133c   ed608c0b           .fphead       n, l, W, BU, br, nosat, 1101011b
00001340       1032           MVK.S1        48,A0
00001342       8000           ADD.L1        A4,A0,A0
00001344       4285           STB.D2T1      A0,*B5[2]
00001346       0427 ||        MVK.L2        0,B0
00001348            $C$L3:
00001348   10004413           CALLP.S2      __divu (PC+544 = 0x00001560),B3
0000134c       81c6 ||        MV.L1         A3,A4
0000134e       4e27 ||        MVK.L2        10,B4
00001350   00101fda           MV.L2X        A4,B0
00001354   0000dec2           ADDAD.D2      B0,0x6,B0
00001358   10007013           CALLP.S2      __c6xabi_remu (PC+896 = 0x000016c0),B3
0000135c   e1600044           .fphead       n, l, W, BU, nobr, nosat, 0001011b
00001360       1285 ||        STB.D2T2      B0,*B5[0]
00001362       4e27 ||        MVK.L2        10,B4
00001364       81c6 ||        MV.L1         A3,A4
00001366       240a           BNOP.S1       $C$L5 (PC+32 = 0x00001380),1
00001368       1032           MVK.S1        48,A0
0000136a       8000           ADD.L1        A4,A0,A0
0000136c       0427           MVK.L2        0,B0
0000136e       5285           STB.D2T2      B0,*B5[2]
00001370       1047 ||        MV.L2X        A0,B0
00001372            $C$L4:
00001372       1032           MVK.S1        48,A0
00001374       6000           ADD.L1        A3,A0,A0
00001376       0285           STB.D2T1      A0,*B5[0]
00001378   0000a35a ||        MVK.L2        0,B0
0000137c   e7e08883           .fphead       n, l, W, BU, br, nosat, 0111111b
00001380            $C$L5:
00001380   00a09362           BNOP.S2X      A8,4
00001384       3285           STB.D2T2      B0,*B5[1]
00001386            GetString_StompDelaySync:
00001386       0246           MV.L1         A4,A0
00001388   00866c28 ||        MVK.S1        0x0cd8,A1
0000138c   00001c41           ADDAW.D1      A0,A0,A0
00001390   00c00068 ||        MVKH.S1       0x80000000,A1
00001394       2050           ADD.L1        A1,A0,A5
00001396       028c           LDB.D1T1      *A5[0],A0
00001398       0626           MVK.L1        0,A4
0000139a       d246           MV.L1X        B4,A6
0000139c   ec410008           .fphead       n, l, W, B, nobr, nosat, 1100010b
000013a0       2c6e           NOP           2
000013a2       a67a    [!A0]  BNOP.S1       $C$L9 (PC+50 = 0x000013d2),5
000013a4       1247           MV.L2X        A4,B0
000013a6       82c6           MV.L1         A5,A4
000013a8       2112 ||        MVK.S1        1,A2
000013aa       3047 ||        MV.L2X        A0,B1
000013ac   a283e000    [ A2]  SPLOOPW       6
000013b0   00002000           NOP           2
000013b4   a09036b7    [ A2]  STB.D2T2      B1,*B4++[1]
000013b8   a1903224 || [ A2]  LDB.D1T1      *++A4[1],A3
000013bc   e0f08018           .fphead       p, l, W, BU, br, nosat, 0000111b
000013c0       31c7           MV.L2X        A3,B1
000013c2       41c6 ||        MV.L1         A3,A2
000013c4       2c6e           NOP           2
000013c6       0c6e           NOP           1
000013c8   00034001           SPKERNEL      0,0
000013cc       2401 ||        ADD.L2        B0,1,B0
000013ce       0c6e           NOP           1
000013d0       9046           MV.L1X        B0,A4
000013d2            $C$L9:
000013d2       61ef           BNOP.S2       B3,3
000013d4       0426           MVK.L1        0,A0
000013d6       c604           STB.D1T1      A0,*A4[A6]
000013d8            GetString_EP3_PreAmp_onf:
000013d8   00100fd9           MV.L1         A4,A0
000013dc   e7600001           .fphead       n, l, W, BU, nobr, nosat, 0111011b
000013e0   00870028 ||        MVK.S1        0x0e00,A1
000013e4   00001c41           ADDAW.D1      A0,A0,A0
000013e8   00c00068 ||        MVKH.S1       0x80000000,A1
000013ec       2050           ADD.L1        A1,A0,A5
000013ee       028c           LDB.D1T1      *A5[0],A0
000013f0       0626           MVK.L1        0,A4
000013f2       d246           MV.L1X        B4,A6
000013f4       2c6e           NOP           2
000013f6       a97a    [!A0]  BNOP.S1       $C$L13 (PC+74 = 0x0000142a),5
000013f8       1247           MV.L2X        A4,B0
000013fa       82c6           MV.L1         A5,A4
000013fc   ef01a000           .fphead       n, l, W, B, br, nosat, 1111000b
00001400       2112 ||        MVK.S1        1,A2
00001402       3047 ||        MV.L2X        A0,B1
00001404   a283e000    [ A2]  SPLOOPW       6
00001408   00002000           NOP           2
0000140c   a09036b7    [ A2]  STB.D2T2      B1,*B4++[1]
00001410   a1903224 || [ A2]  LDB.D1T1      *++A4[1],A3
00001414       31c7           MV.L2X        A3,B1
00001416       41c6 ||        MV.L1         A3,A2
00001418       2c6e           NOP           2
0000141a       0c6e           NOP           1
0000141c   ec300401           .fphead       p, l, W, BU, nobr, nosat, 1100001b
00001420   00034001           SPKERNEL      0,0
00001424       2401 ||        ADD.L2        B0,1,B0
00001426       0c6e           NOP           1
00001428       9046           MV.L1X        B0,A4
0000142a            $C$L13:
0000142a       61ef           BNOP.S2       B3,3
0000142c       0426           MVK.L1        0,A0
0000142e       c604           STB.D1T1      A0,*A4[A6]
00001430            Dll_TapeEcho3:
00001430   0005402a           MVK.S2        0x0a80,B0
00001434       01ef           BNOP.S2       B3,0
00001436       0c26 ||        MVK.L1        8,A0
00001438   00860029           MVK.S1        0x0c00,A1
0000143c   e5c00400           .fphead       n, l, W, BU, nobr, nosat, 0101110b
00001440   0040006b ||        MVKH.S2       0x80000000,B0
00001444       0204 ||        STB.D1T1      A0,*A4[0]
00001446       3004           STW.D1T2      B0,*A4[1]
00001448   00c00069 ||        MVKH.S1       0x80000000,A1
0000144c   0083f02a ||        MVK.S2        0x07e0,B1
00001450   0103dc29           MVK.S1        0x07b8,A2
00001454   0080006b ||        MVKH.S2       0x0000,B1
00001458       4014 ||        STW.D1T1      A1,*A4[2]
0000145a       b014           STW.D1T2      B1,*A4[5]
0000145c   e8402008           .fphead       n, l, W, BU, nobr, nosat, 1000010b
00001460   01000068 ||        MVKH.S1       0x0000,A2
00001464   01108274           STW.D1T1      A2,*+A4[4]
00001468   00000000           NOP
0000146c   00000000           NOP
00001470   00000000           NOP
00001474   00000000           NOP
00001478   00000000           NOP
0000147c   00000000           NOP
00001480            __divi:
00001480            __c6xabi_divi:
00001480   029005a3           NEG.S2        B4,B5
00001484   053c54f5 ||        STW.D2T1      A10,*B15--[2]
00001488   0500a359 ||        MVK.L1        0,A10
0000148c   00902d5a ||        LMBD.L2       1,B4,B1
00001490   01148f7b           AND.L2        B4,B5,B2
00001494   05bc22f5 ||        STW.D2T1      A11,*+B15[1]
00001498   05900fd9 ||        MV.L1         A4,A11
0000149c   50902ca2 || [!B1]  SHL.S2        B4,0x1,B1
000014a0       a569           CMPEQ.L2      B5,B2,B0
000014a2       a0d7 ||        MV.D2         B1,B5
000014a4   0093e9a1 ||        SHRU.S1       A4,0x1f,A1
000014a8   0093e9a2 ||        SHRU.S2       B4,0x1f,B1
000014ac   25282941    [ B0]  ADD.D1        A10,0x1,A10
000014b0   001408f3 ||        MV.D2         B5,B0
000014b4   01088a7b ||        CMPEQ.L2      B4,B2,B2
000014b8   821000d9 || [ A1]  NEG.L1        A4,A4
000014bc   e0200003           .fphead       n, l, W, BU, nobr, nosat, 0000001b
000014c0   421005a3 || [ B1]  NEG.S2        B4,B4
000014c4   00000990 ||        B.S1          LOOP (PC+76 = 0x0000150c)
000014c8   657fffa9    [ B2]  MVK.S1        0xffffffff,A10
000014cc   01100c79 ||        NORM.L1       A4,A2
000014d0   01100c7b ||        NORM.L2       B4,B2
000014d4       c0d6 ||        MV.D1         A1,A6
000014d6       a0d7 ||        MV.D2         B1,B5
000014d8       098b ||        BNOP.S2       LOOP (PC+76 = 0x0000150c),0
000014da       9e58           CMPLTU.L1X    A4,B4,A1
000014dc   ec00ac00           .fphead       n, l, W, BU, br, nosat, 1100000b
000014e0       5901 ||        SUB.L2X       B2,A2,B0
000014e2       f812 ||        MVK.S1        31,A0
000014e4   00000593 ||        B.S2          LOOP (PC+44 = 0x0000150c)
000014e8   35000040 || [!B0]  MVK.D1        0,A10
000014ec   02100ce3           SHL.S2        B4,B0,B4
000014f0   0100c8db ||        CMPGT.L2      6,B0,B2
000014f4   0080c9c3 ||        SUB.D2        B0,0x6,B1
000014f8       1800 ||        SUB.L1X       A0,B0,A0
000014fa       058a ||        BNOP.S1       LOOP (PC+44 = 0x0000150c),0
000014fc   e8209003           .fphead       n, l, W, BU, br, nosat, 1000001b
00001500   60800043    [ B2]  MVK.D2        0,B1
00001504   02109979 ||        SUBC.L1X      A4,B4,A4
00001508   00000192 ||        B.S2          LOOP (PC+12 = 0x0000150c)
0000150c            LOOP:
0000150c   22109979    [ B0]  SUBC.L1X      A4,B4,A4
00001510   2003e05b || [ B0]  SUB.L2        B0,0x1,B0
00001514   408429c3 || [ B1]  SUB.D2        B1,0x1,B1
00001518   40000190 || [ B1]  B.S1          LOOP (PC+12 = 0x0000150c)
0000151c   000c0363           B.S2          B3
00001520   05bc22e5 ||        LDW.D2T1      *+B15[1],A11
00001524   0100a35a ||        MVK.L2        0,B2
00001528   92100ce1    [!A1]  SHL.S1        A4,A0,A4
0000152c   82000041 || [ A1]  MVK.D1        0,A4
00001530   0114ddf9 ||        XOR.L1X       A6,B5,A2
00001534   053c52e5 ||        LDW.D2T1      *++B15[2],A10
00001538   0140006a ||        MVKH.S2       0x80000000,B2
0000153c   921009e1    [!A1]  SHRU.S1       A4,A0,A4
00001540   002c5a7a ||        CMPEQ.L2X     B2,A11,B0
00001544   a21005a1    [ A2]  NEG.S1        A4,A4
00001548   3500a358 || [!B0]  MVK.L1        0,A10
0000154c   01280fd8           MV.L1         A10,A2
00001550   a2088078    [ A2]  ADD.L1        A4,A2,A4
00001554   00000000           NOP
00001558   00000000           NOP
0000155c   00000000           NOP
00001560            __divu:
00001560            __c6xabi_divu:
00001560   00903d5b           LMBD.L2X      1,A4,B1
00001564   00903d59 ||        LMBD.L1X      1,B4,A1
00001568       0032 ||        MVK.S1        32,A0
0000156a       1976 ||        MVK.D1        0,A2
0000156c   00909bf9           CMPLTU.L1X    A4,B4,A1
00001570   00043d73 ||        SUB.S2X       A1,B1,B0
00001574   51002040 || [!B1]  MVK.D1        1,A2
00001578   02100ce3           SHL.S2        B4,B0,B4
0000157c   e0800010           .fphead       n, l, W, BU, nobr, nosat, 0000100b
00001580   40002943 || [ B1]  ADD.D2        B0,0x1,B0
00001584   030018f0 ||        MV.D1X        B0,A6
00001588   011099fb           CMPGTU.L2X    B4,A4,B2
0000158c       1836 ||        SUB.D1X       A0,B0,A0
0000158e       c562 ||        SHL.S1        A2,A6,A2
00001590   00000c12 ||        B.S2          LOOP (PC+96 = 0x000015e0)
00001594   4100a35b    [ B1]  MVK.L2        0,B2
00001598   608808f3 || [ B2]  MV.D2         B2,B1
0000159c   e10000c0           .fphead       n, l, W, BU, nobr, nosat, 0001000b
000015a0   610829a1 || [ B2]  SHRU.S1       A2,0x1,A2
000015a4   00000812 ||        B.S2          LOOP (PC+64 = 0x000015e0)
000015a8   621029a3    [ B2]  SHRU.S2       B4,0x1,B4
000015ac   52109b31 || [!B1]  SUB.D1X       A4,B4,A4
000015b0   00000810 ||        B.S1          LOOP (PC+64 = 0x000015e0)
000015b4   521029a3    [!B1]  SHRU.S2       B4,0x1,B4
000015b8   62109b31 || [ B2]  SUB.D1X       A4,B4,A4
000015bc   0100e8db ||        CMPGT.L2      7,B0,B2
000015c0   0080e9c3 ||        SUB.D2        B0,0x7,B1
000015c4   00000410 ||        B.S1          LOOP (PC+32 = 0x000015e0)
000015c8   6080a35b    [ B2]  MVK.L2        0,B1
000015cc   22109979 || [ B0]  SUBC.L1X      A4,B4,A4
000015d0   200029c3 || [ B0]  SUB.D2        B0,0x1,B0
000015d4   00000413 ||        B.S2          LOOP (PC+32 = 0x000015e0)
000015d8   00000001 ||        NOP
000015dc   00000000 ||        NOP
000015e0            LOOP:
000015e0   22109979    [ B0]  SUBC.L1X      A4,B4,A4
000015e4   200029c3 || [ B0]  SUB.D2        B0,0x1,B0
000015e8   4087e05b || [ B1]  SUB.L2        B1,0x1,B1
000015ec   40000012 || [ B1]  B.S2          LOOP (PC+0 = 0x000015e0)
000015f0   000c0362           B.S2          B3
000015f4   92100ce1    [!A1]  SHL.S1        A4,A0,A4
000015f8   8200a358 || [ A1]  MVK.L1        0,A4
000015fc   921009e0    [!A1]  SHRU.S1       A4,A0,A4
00001600   92104840    [!A1]  ADD.D1        A4,A2,A4
00001604   00002000           NOP           2
00001608   00000000           NOP
0000160c   00000000           NOP
00001610   00000000           NOP
00001614   00000000           NOP
00001618   00000000           NOP
0000161c   00000000           NOP
00001620            Fx_DLY_TapeEcho3:
00001620       25f7           STW.D2T1      A11,*B15--[2]
00001622       9577           STDW.D2T2     B11:B10,*B15--[1]
00001624       2577           STW.D2T1      A10,*B15--[2]
00001626       6646 ||        MV.L1         A4,A11
00001628   052c2266           LDW.D1T2      *+A11[1],B10
0000162c   052c4264           LDW.D1T1      *+A11[2],A10
00001630   0280ac2a           MVK.S2        0x0158,B5
00001634   02010c2a           MVK.S2        0x0218,B4
00001638   01811828           MVK.S1        0x0230,A3
0000163c   e0600004           .fphead       n, l, W, BU, nobr, nosat, 0000011b
00001640   02a8a07a           ADD.L2        B5,B10,B5
00001644       0646           MV.L1         A4,A8
00001646       65c7           MV.L2         B3,B11
00001648   1ffee813 ||        CALLP.S2      SUB_TapeEcho3 (PC-2240 = 0x00000d80),B3
0000164c   032c6267 ||        LDW.D1T2      *+A11[3],B6
00001650   02289ab3 ||        ADD.D2X       B4,A10,B4
00001654   03286079 ||        ADD.L1        A3,A10,A6
00001658   021416a0 ||        MV.S1X        B5,A4
0000165c   e0400008           .fphead       n, l, W, BU, nobr, nosat, 0000010b
00001660   022a8942           ADD.D2        B10,0x14,B4
00001664       d587           MV.L2X        A11,B6
00001666       9246           MV.L1X        B4,A4
00001668   1ffd3413 ||        CALLP.S2      SUB_KawaOD_EP_Booster (PC-5728 = 0x00000000),B3
0000166c   032881a1 ||        ADD.S1        4,A10,A6
00001670   02281fda ||        MV.L2X        A10,B4
00001674   01aca264           LDW.D1T1      *+A11[5],A3
00001678       a507           MV.L2         B10,B5
0000167a       e427           MVK.L2        7,B0
0000167c   e8400008           .fphead       n, l, W, BU, nobr, nosat, 1000010b
00001680       2c6e           NOP           2
00001682       8dc0           ADD.L1        A3,-4,A4
00001684            $C$L2:
00001684   01903265           LDW.D1T1      *++A4[1],A3
00001688       90cd ||        LDW.D2T2      *B5[4],B4
0000168a       6c6e           NOP           4
0000168c   01907e00           MPYSP.M1X     A3,B4,A3
00001690       4c6e           NOP           3
00001692       0034           STW.D1T1      A3,*A4[0]
00001694       083c           LDW.D1T1      *A4[8],A3
00001696       90cd ||        LDW.D2T2      *B5[4],B4
00001698   00004000           NOP           3
0000169c   e6a00400           .fphead       n, l, W, BU, nobr, nosat, 0110101b
000016a0   207f3022    [ B0]  BDEC.S2       $C$L2 (PC-28 = 0x00001684),B0
000016a4   020c9e02           MPYSP.M2X     B4,A3,B4
000016a8       4c6e           NOP           3
000016aa       1844           STW.D1T2      B4,*A4[8]
000016ac       6577           LDW.D2T1      *++B15[2],A10
000016ae       6587 ||        MV.L2         B11,B3
000016b0       d577           LDDW.D2T2     *++B15[1],B11:B10
000016b2       01ef ||        BNOP.S2       B3,0
000016b4       65f7           LDW.D2T1      *++B15[2],A11
000016b6       6c6e           NOP           4
000016b8   00000000           NOP
000016bc   e7800140           .fphead       n, l, W, BU, nobr, nosat, 0111100b
000016c0            __c6xabi_remu:
000016c0            __remu:
000016c0   00903d5b           LMBD.L2X      1,A4,B1
000016c4   00903d58 ||        LMBD.L1X      1,B4,A1
000016c8   00909bf9           CMPLTU.L1X    A4,B4,A1
000016cc   00043d73 ||        SUB.S2X       A1,B1,B0
000016d0       a256 ||        MV.D1         A4,A5
000016d2       0663           SHL.S2        B4,B0,B4
000016d4   40002942 || [ B1]  ADD.D2        B0,0x1,B0
000016d8   011099fb           CMPGTU.L2X    B4,A4,B2
000016dc   e2000200           .fphead       n, l, W, BU, nobr, nosat, 0010000b
000016e0   00000892 ||        B.S2          LOOP (PC+68 = 0x00001724)
000016e4   4100a35b    [ B1]  MVK.L2        0,B2
000016e8   608808f3 || [ B2]  MV.D2         B2,B1
000016ec       f056 ||        MV.D1X        B0,A7
000016ee       088b ||        BNOP.S2       LOOP (PC+68 = 0x00001724),0
000016f0   621029a3    [ B2]  SHRU.S2       B4,0x1,B4
000016f4   52109b31 || [!B1]  SUB.D1X       A4,B4,A4
000016f8   00000890 ||        B.S1          LOOP (PC+68 = 0x00001724)
000016fc   e1008040           .fphead       n, l, W, BU, br, nosat, 0001000b
00001700   521029a3    [!B1]  SHRU.S2       B4,0x1,B4
00001704   62109b31 || [ B2]  SUB.D1X       A4,B4,A4
00001708   0100e8db ||        CMPGT.L2      7,B0,B2
0000170c   0080e9c3 ||        SUB.D2        B0,0x7,B1
00001710   00000490 ||        B.S1          LOOP (PC+36 = 0x00001724)
00001714   6080a35b    [ B2]  MVK.L2        0,B1
00001718   22109979 || [ B0]  SUBC.L1X      A4,B4,A4
0000171c   200029c3 || [ B0]  SUB.D2        B0,0x1,B0
00001720   00000092 ||        B.S2          LOOP (PC+4 = 0x00001724)
00001724            LOOP:
00001724   22109979    [ B0]  SUBC.L1X      A4,B4,A4
00001728   200029c3 || [ B0]  SUB.D2        B0,0x1,B0
0000172c   4087e05b || [ B1]  SUB.L2        B1,0x1,B1
00001730   40000092 || [ B1]  B.S2          LOOP (PC+4 = 0x00001724)
00001734   9210e9e1    [!A1]  SHRU.S1       A4,A7,A4
00001738   821408f1 || [ A1]  MV.D1         A5,A4
0000173c   000c0362 ||        B.S2          B3
00001740   00008000           NOP           5
00001744   00000000           NOP
00001748   00000000           NOP
0000174c   00000000           NOP
00001750   00000000           NOP
00001754   00000000           NOP
00001758   00000000           NOP
0000175c   00000000           NOP
00001760            __call_stub:
00001760            __c6xabi_call_stub:
00001760   013c54f4           STW.D2T1      A2,*B15--[2]
00001764   007c0363           B.S2          B31
00001768       8377 ||        STDW.D2T1     A7:A6,*B15--[1]
0000176a       8077           STDW.D2T1     A1:A0,*B15--[1]
0000176c       9377           STDW.D2T2     B7:B6,*B15--[1]
0000176e       9277           STDW.D2T2     B5:B4,*B15--[1]
00001770       9077           STDW.D2T2     B1:B0,*B15--[1]
00001772       9177           STDW.D2T2     B3:B2,*B15--[1]
00001774   01860162 ||        ADDKPC.S2     __stub_ret (PC+24 = 0x00001778),B3,0
00001778            __stub_ret:
00001778       d177           LDDW.D2T2     *++B15[1],B3:B2
0000177a       d077           LDDW.D2T2     *++B15[1],B1:B0
0000177c   eb800200           .fphead       n, l, W, BU, nobr, nosat, 1011100b
00001780   023c33e6           LDDW.D2T2     *++B15[1],B5:B4
00001784   033c33e6           LDDW.D2T2     *++B15[1],B7:B6
00001788   003c33e4           LDDW.D2T1     *++B15[1],A1:A0
0000178c   000c0363           B.S2          B3
00001790   033c33e4 ||        LDDW.D2T1     *++B15[1],A7:A6
00001794   013c52e4           LDW.D2T1      *++B15[2],A2
00001798   00006000           NOP           4
0000179c   00000000           NOP
000017a0            Fx_DLY_TapeEcho3_PreAmp_edit:
000017a0   01bc54f7           STW.D2T2      B3,*B15--[2]
000017a4   1ffe2892 ||        CALLP.S2      Fx_DLY_TapeEcho3_Booster_onf (PC-3772 = 0x000008e4),B3
000017a8   01bc52e6           LDW.D2T2      *++B15[2],B3
000017ac   00006000           NOP           4
000017b0   008ca362           BNOP.S2       B3,5
000017b4   00000000           NOP
000017b8   00000000           NOP
000017bc   00000000           NOP
000017c0            Fx_DLY_TapeEcho3_fb_edit:
000017c0   01bc54f7           STW.D2T2      B3,*B15--[2]
000017c4   1ffe6c12 ||        CALLP.S2      Fx_DLY_TapeEcho3_RecLvl_edit (PC-3232 = 0x00000b20),B3
000017c8   01bc52e6           LDW.D2T2      *++B15[2],B3
000017cc   00006000           NOP           4
000017d0   008ca362           BNOP.S2       B3,5
000017d4   00000000           NOP
000017d8   00000000           NOP
000017dc   00000000           NOP
000017e0            __c6xabi_pop_rts:
000017e0            __pop_rts:
000017e0       d177           LDDW.D2T2     *++B15[1],B3:B2
000017e2       c577           LDDW.D2T1     *++B15[1],A11:A10
000017e4       d577           LDDW.D2T2     *++B15[1],B11:B10
000017e6       c677           LDDW.D2T1     *++B15[1],A13:A12
000017e8       d677           LDDW.D2T2     *++B15[1],B13:B12
000017ea       01ef           BNOP.S2       B3,0
000017ec       c777 ||        LDDW.D2T1     *++B15[1],A15:A14
000017ee       7777           LDW.D2T2      *++B15[2],B14
000017f0   00006000           NOP           4
000017f4   00000000           NOP
000017f8   00000000           NOP
000017fc   e1e00020           .fphead       n, l, W, BU, nobr, nosat, 0001111b
00001800            __push_rts:
00001800            __c6xabi_push_rts:
00001800   073c54f6           STW.D2T2      B14,*B15--[2]
00001804   000c1363           B.S2X         A3
00001808       8777 ||        STDW.D2T1     A15:A14,*B15--[1]
0000180a       9677           STDW.D2T2     B13:B12,*B15--[1]
0000180c       8677           STDW.D2T1     A13:A12,*B15--[1]
0000180e       9577           STDW.D2T2     B11:B10,*B15--[1]
00001810       8577           STDW.D2T1     A11:A10,*B15--[1]
00001812       9177           STDW.D2T2     B3:B2,*B15--[1]
00001814   00000000           NOP
00001818   00000000           NOP
0000181c   e3800000           .fphead       n, l, W, BU, nobr, nosat, 0011100b

DATA Section .const (Little Endian), 0xe0a bytes at 0x80000000
80000000            picEffectType_TapeEcho3:
80000000   fc000000           .word 0xfc000000
80000004   3c3c02fc           .word 0x3c3c02fc
80000008   3c3cfc07           .word 0x3c3cfc07
8000000c   01fcfc04           .word 0x01fcfc04
80000010   7c053c3c           .word 0x7c053c3c
80000014   3c00fcfc           .word 0x3c00fcfc
80000018   fcfc043c           .word 0xfcfc043c
8000001c   043c3c03           .word 0x043c3c03
80000020   7c01fcfc           .word 0x7c01fcfc
80000024   7c033c3c           .word 0x7c033c3c
80000028   3c00fcfc           .word 0x3c00fcfc
8000002c   7cfc013c           .word 0x7cfc013c
80000030   013c3cfc           .word 0x013c3cfc
80000034   ec00dcdc           .word 0xec00dcdc
80000038   012c2c6c           .word 0x012c2c6c
8000003c   f4f47434           .word 0xf4f47434
80000040   34347401           .word 0x34347401
80000044   ecec7403           .word 0xecec7403
80000048   01dcdc03           .word 0x01dcdc03
8000004c   7c00bcbc           .word 0x7c00bcbc
80000050   fcfc007c           .word 0xfcfc007c
80000054   02000014           .word 0x02000014
80000058   0005ffff           .word 0x0005ffff
8000005c   ffff0100           .word 0xffff0100
80000060   00000f01           .word 0x00000f01
80000064   c0ffc000           .word 0xc0ffc000
80000068   ffff0300           .word 0xffff0300
8000006c   01000000           .word 0x01000000
80000070   80009f9f           .word 0x80009f9f
80000074   ffc00080           .word 0xffc00080
80000078   000000ff           .word 0x000000ff
8000007c   01cfcf01           .word 0x01cfcf01
80000080   0003ffff           .word 0x0003ffff
80000084   cfcf0100           .word 0xcfcf0100
80000088   00ffff01           .word 0x00ffff01
8000008c   ff010000           .word 0xff010000
80000090   303001ff           .word 0x303001ff
80000094   0000ff01           .word 0x0000ff01
80000098   019f9f01           .word 0x019f9f01
8000009c   ff010000           .word 0xff010000
800000a0   000000ff           .word 0x000000ff
800000a4   00ffff01           .word 0x00ffff01
800000a8   ff010000           .word 0xff010000
800000ac   487868ef           .word 0x487868ef
800000b0   0000cece           .word 0x0000cece
800000b4   ff100000           .word 0xff100000
800000b8   f3fb7b3d           .word 0xf3fb7b3d
800000bc   dfdfeff7           .word 0xdfdfeff7
800000c0   fe7fbf00           .word 0xfe7fbf00
800000c4   fb00fdfd           .word 0xfb00fdfd
800000c8   3fdfeff7           .word 0x3fdfeff7
800000cc   000cffff           .word 0x000cffff
800000d0   ffff0200           .word 0xffff0200
800000d4   01e0e005           .word 0x01e0e005
800000d8   e001ffff           .word 0xe001ffff
800000dc   fcfc01e0           .word 0xfcfc01e0
800000e0   01e0e001           .word 0x01e0e001
800000e4   01e0e0ff           .word 0x01e0e0ff
800000e8   e005ffff           .word 0xe005ffff
800000ec   e7e701e0           .word 0xe7e701e0
800000f0   02ffff02           .word 0x02ffff02
800000f4   e701e0e0           .word 0xe701e0e0
800000f8   f8ff02e7           .word 0xf8ff02e7
800000fc   67e7e0f0           .word 0x67e7e0f0
80000100   f8f0e0e7           .word 0xf8f0e0e7
80000104   00e0e0ff           .word 0x00e0e0ff
80000108   fffcff20           .word 0xfffcff20
8000010c   2000e0e0           .word 0x2000e0e0
80000110   f0f8ffdf           .word 0xf0f8ffdf
80000114   00e7e7e0           .word 0x00e7e7e0
80000118   ffd8b060           .word 0xffd8b060
8000011c   f0f800ff           .word 0xf0f800ff
80000120   00e7e7e0           .word 0x00e7e7e0
80000124   fff870e0           .word 0xfff870e0
80000128   fffe00ff           .word 0xfffe00ff
8000012c   ffff0bfd           .word 0xffff0bfd
80000130   07f9fe02           .word 0x07f9fe02
80000134   f804ffff           .word 0xf804ffff
80000138   0affff07           .word 0x0affff07
8000013c   fd020000           .word 0xfd020000
80000140   fdfc36fd           .word 0xfdfc36fd
80000144   fdfc05fd           .word 0xfdfc05fd
80000148   058d06fd           .word 0x058d06fd
8000014c   25250005           .word 0x25250005
80000150   00050500           .word 0x00050500
80000154   74077575           .word 0x74077575
80000158   740b7575           .word 0x740b7575
8000015c   74057575           .word 0x74057575
80000160   05017575           .word 0x05017575
80000164   05fdfd8d           .word 0x05fdfd8d
80000168   fd020000           .word 0xfd020000
8000016c   ad5d00fd           .word 0xad5d00fd
80000170   ad5dad5d           .word 0xad5dad5d
80000174   ad5dad5d           .word 0xad5dad5d
80000178   ad5dad5d           .word 0xad5dad5d
8000017c   ad5dad5d           .word 0xad5dad5d
80000180   ad5dad5d           .word 0xad5dad5d
80000184   ad5dad5d           .word 0xad5dad5d
80000188   edddaddd           .word 0xedddaddd
8000018c   ed5d6ddd           .word 0xed5d6ddd
80000190   adddeddd           .word 0xadddeddd
80000194   ad5dad5d           .word 0xad5dad5d
80000198   ad5dad5d           .word 0xad5dad5d
8000019c   ad5dad5d           .word 0xad5dad5d
800001a0   adddad5d           .word 0xadddad5d
800001a4   6dddeddd           .word 0x6dddeddd
800001a8   eddded5d           .word 0xeddded5d
800001ac   ad5daddd           .word 0xad5daddd
800001b0   ad5dad5d           .word 0xad5dad5d
800001b4   fcfc0d5d           .word 0xfcfc0d5d
800001b8   036d6d00           .word 0x036d6d00
800001bc   6d00eded           .word 0x6d00eded
800001c0   eded036d           .word 0xeded036d
800001c4   036d6d00           .word 0x036d6d00
800001c8   6d00eded           .word 0x6d00eded
800001cc   eded036d           .word 0xeded036d
800001d0   036d6d00           .word 0x036d6d00
800001d4   0d00eded           .word 0x0d00eded
800001d8   ad5dad5d           .word 0xad5dad5d
800001dc   ad5dad5d           .word 0xad5dad5d
800001e0   fdfdad5d           .word 0xfdfdad5d
800001e4   02000000           .word 0x02000000
800001e8   5500ffff           .word 0x5500ffff
800001ec   55aa55aa           .word 0x55aa55aa
800001f0   95aa952a           .word 0x95aa952a
800001f4   15aa95aa           .word 0x15aa95aa
800001f8   55aa552a           .word 0x55aa552a
800001fc   55aa55aa           .word 0x55aa55aa
80000200   8ffefdaa           .word 0x8ffefdaa
80000204   fdfdfb77           .word 0xfdfdfb77
80000208   00fefe00           .word 0x00fefe00
8000020c   fe00c0c0           .word 0xfe00c0c0
80000210   fdfd00fe           .word 0xfdfd00fe
80000214   8e77fb00           .word 0x8e77fb00
80000218   aa55fafd           .word 0xaa55fafd
8000021c   aa55aa55           .word 0xaa55aa55
80000220   778ffefd           .word 0x778ffefd
80000224   00fdfdfb           .word 0x00fdfdfb
80000228   c000fefe           .word 0xc000fefe
8000022c   fefe00c0           .word 0xfefe00c0
80000230   00fdfd00           .word 0x00fdfd00
80000234   fd8e77fb           .word 0xfd8e77fb
80000238   55aa55fa           .word 0x55aa55fa
8000023c   50a757a0           .word 0x50a757a0
80000240   50aa55aa           .word 0x50aa55aa
80000244   55a057a7           .word 0x55a057a7
80000248   97a0152a           .word 0x97a0152a
8000024c   95aa10a7           .word 0x95aa10a7
80000250   97a710aa           .word 0x97a710aa
80000254   15aa9520           .word 0x15aa9520
80000258   10a797a0           .word 0x10a797a0
8000025c   50aa552a           .word 0x50aa552a
80000260   55a057a7           .word 0x55a057a7
80000264   55aa55aa           .word 0x55aa55aa
80000268   55aa55aa           .word 0x55aa55aa
8000026c   00ffffaa           .word 0x00ffffaa
80000270   ff020000           .word 0xff020000
80000274   aa5500ff           .word 0xaa5500ff
80000278   633e9c01           .word 0x633e9c01
8000027c   d5d5ddc1           .word 0xd5d5ddc1
80000280   63c1dd00           .word 0x63c1dd00
80000284   55801c3e           .word 0x55801c3e
80000288   55aa55aa           .word 0x55aa55aa
8000028c   57ab55aa           .word 0x57ab55aa
80000290   dd0d1e8f           .word 0xdd0d1e8f
80000294   1b005b5b           .word 0x1b005b5b
80000298   5bdb001b           .word 0x5bdb001b
8000029c   570e1d9d           .word 0x570e1d9d
800002a0   55aa55ab           .word 0x55aa55ab
800002a4   55aa55aa           .word 0x55aa55aa
800002a8   170b55aa           .word 0x170b55aa
800002ac   dd8ddecf           .word 0xdd8ddecf
800002b0   db5b1bdb           .word 0xdb5b1bdb
800002b4   9ddd1b5b           .word 0x9ddd1b5b
800002b8   550b17ce           .word 0x550b17ce
800002bc   55aa55aa           .word 0x55aa55aa
800002c0   55aa55aa           .word 0x55aa55aa
800002c4   55aa55aa           .word 0x55aa55aa
800002c8   55aa55aa           .word 0x55aa55aa
800002cc   4fa040a0           .word 0x4fa040a0
800002d0   4fa840a0           .word 0x4fa840a0
800002d4   41af40a8           .word 0x41af40a8
800002d8   40af41a7           .word 0x40af41a7
800002dc   40aa4aaf           .word 0x40aa4aaf
800002e0   55aa55a0           .word 0x55aa55a0
800002e4   55aa55aa           .word 0x55aa55aa
800002e8   55aa55aa           .word 0x55aa55aa
800002ec   55aa55aa           .word 0x55aa55aa
800002f0   00ffffaa           .word 0x00ffffaa
800002f4   3f020000           .word 0x3f020000
800002f8   3a35003f           .word 0x3a35003f
800002fc   3a343a35           .word 0x3a343a35
80000300   3a343a34           .word 0x3a343a34
80000304   3a343a34           .word 0x3a343a34
80000308   3a353a35           .word 0x3a353a35
8000030c   3a353a35           .word 0x3a353a35
80000310   3a353a35           .word 0x3a353a35
80000314   37003030           .word 0x37003030
80000318   30343031           .word 0x30343031
8000031c   30363537           .word 0x30363537
80000320   3a350030           .word 0x3a350030
80000324   3a353a35           .word 0x3a353a35
80000328   3a353a35           .word 0x3a353a35
8000032c   00303035           .word 0x00303035
80000330   30333037           .word 0x30333037
80000334   37343037           .word 0x37343037
80000338   33363034           .word 0x33363034
8000033c   00303036           .word 0x00303036
80000340   3a353a35           .word 0x3a353a35
80000344   3a353a35           .word 0x3a353a35
80000348   3a353a35           .word 0x3a353a35
8000034c   3a353a35           .word 0x3a353a35
80000350   3a353a35           .word 0x3a353a35
80000354   3a353a35           .word 0x3a353a35
80000358   3a353a35           .word 0x3a353a35
8000035c   3a353a35           .word 0x3a353a35
80000360   3a353a35           .word 0x3a353a35
80000364   3a353a35           .word 0x3a353a35
80000368   3a353a35           .word 0x3a353a35
8000036c   3a353a35           .word 0x3a353a35
80000370   3a353a35           .word 0x3a353a35
80000374   3f3f3a35           .word 0x3f3f3a35
80000378   00000000           .word 0x00000000
8000037c   00000000           .word 0x00000000
80000380            picEffectTypeKnob_TapeEcho3:
80000380   c2850a15           .word 0xc2850a15
80000384   d8b870e1           .word 0xd8b870e1
80000388   ecececdc           .word 0xecececdc
8000038c   dcececec           .word 0xdcececec
80000390   e271b8d8           .word 0xe271b8d8
80000394   2a158ac5           .word 0x2a158ac5
80000398   1f0f8040           .word 0x1f0f8040
8000039c   dfef7738           .word 0xdfef7738
800003a0   b3a78f9f           .word 0xb3a78f9f
800003a4   dfbfbfbf           .word 0xdfbfbfbf
800003a8   3877efdf           .word 0x3877efdf
800003ac   a0408f1f           .word 0xa0408f1f
800003b0   00010001           .word 0x00010001
800003b4   00000000           .word 0x00000000
800003b8   01010101           .word 0x01010101
800003bc   01010101           .word 0x01010101
800003c0   00000000           .word 0x00000000
800003c4   00010001           .word 0x00010001
800003c8   c2850a15           .word 0xc2850a15
800003cc   d8b870e1           .word 0xd8b870e1
800003d0   ecececdc           .word 0xecececdc
800003d4   dcececec           .word 0xdcececec
800003d8   e271b8d8           .word 0xe271b8d8
800003dc   2a158ac5           .word 0x2a158ac5
800003e0   1f0f8040           .word 0x1f0f8040
800003e4   d7e76738           .word 0xd7e76738
800003e8   bdb9bbd3           .word 0xbdb9bbd3
800003ec   dfbfbfbf           .word 0xdfbfbfbf
800003f0   3877efdf           .word 0x3877efdf
800003f4   a0408f1f           .word 0xa0408f1f
800003f8   00010001           .word 0x00010001
800003fc   00000000           .word 0x00000000
80000400   01010101           .word 0x01010101
80000404   01010101           .word 0x01010101
80000408   00000000           .word 0x00000000
8000040c   00010001           .word 0x00010001
80000410   c2850a15           .word 0xc2850a15
80000414   d8b870e1           .word 0xd8b870e1
80000418   ecececdc           .word 0xecececdc
8000041c   dcececec           .word 0xdcececec
80000420   e271b8d8           .word 0xe271b8d8
80000424   2a158ac5           .word 0x2a158ac5
80000428   1d0f8040           .word 0x1d0f8040
8000042c   dded7538           .word 0xdded7538
80000430   bdbdbddd           .word 0xbdbdbddd
80000434   dfbfbfbf           .word 0xdfbfbfbf
80000438   3877efdf           .word 0x3877efdf
8000043c   a0408f1f           .word 0xa0408f1f
80000440   00010001           .word 0x00010001
80000444   00000000           .word 0x00000000
80000448   01010101           .word 0x01010101
8000044c   01010101           .word 0x01010101
80000450   00000000           .word 0x00000000
80000454   00010001           .word 0x00010001
80000458   c2850a15           .word 0xc2850a15
8000045c   583830e1           .word 0x583830e1
80000460   ececec5c           .word 0xececec5c
80000464   dcececec           .word 0xdcececec
80000468   e271b8d8           .word 0xe271b8d8
8000046c   2a158ac5           .word 0x2a158ac5
80000470   1f0f8040           .word 0x1f0f8040
80000474   dfef7738           .word 0xdfef7738
80000478   bdbcbede           .word 0xbdbcbede
8000047c   dfbfbfbf           .word 0xdfbfbfbf
80000480   3877efdf           .word 0x3877efdf
80000484   a0408f1f           .word 0xa0408f1f
80000488   00010001           .word 0x00010001
8000048c   00000000           .word 0x00000000
80000490   01010101           .word 0x01010101
80000494   01010101           .word 0x01010101
80000498   00000000           .word 0x00000000
8000049c   00010001           .word 0x00010001
800004a0   c2850a15           .word 0xc2850a15
800004a4   d8b870e1           .word 0xd8b870e1
800004a8   6c2c8ccc           .word 0x6c2c8ccc
800004ac   dcececec           .word 0xdcececec
800004b0   e271b8d8           .word 0xe271b8d8
800004b4   2a158ac5           .word 0x2a158ac5
800004b8   1f0f8040           .word 0x1f0f8040
800004bc   dfef7738           .word 0xdfef7738
800004c0   bebfbfdf           .word 0xbebfbfdf
800004c4   dfbfbfbf           .word 0xdfbfbfbf
800004c8   3877efdf           .word 0x3877efdf
800004cc   a0408f1f           .word 0xa0408f1f
800004d0   00010001           .word 0x00010001
800004d4   00000000           .word 0x00000000
800004d8   01010101           .word 0x01010101
800004dc   01010101           .word 0x01010101
800004e0   00000000           .word 0x00000000
800004e4   00010001           .word 0x00010001
800004e8   c2850a15           .word 0xc2850a15
800004ec   d8b870e1           .word 0xd8b870e1
800004f0   04ececdc           .word 0x04ececdc
800004f4   dcecec04           .word 0xdcecec04
800004f8   e271b8d8           .word 0xe271b8d8
800004fc   2a158ac5           .word 0x2a158ac5
80000500   1f0f8040           .word 0x1f0f8040
80000504   dfef7738           .word 0xdfef7738
80000508   bcbfbfdf           .word 0xbcbfbfdf
8000050c   dfbfbfbc           .word 0xdfbfbfbc
80000510   3877efdf           .word 0x3877efdf
80000514   a0408f1f           .word 0xa0408f1f
80000518   00010001           .word 0x00010001
8000051c   00000000           .word 0x00000000
80000520   01010101           .word 0x01010101
80000524   01010101           .word 0x01010101
80000528   00000000           .word 0x00000000
8000052c   00010001           .word 0x00010001
80000530   c2850a15           .word 0xc2850a15
80000534   d8b870e1           .word 0xd8b870e1
80000538   ecececdc           .word 0xecececdc
8000053c   cc8c2c6c           .word 0xcc8c2c6c
80000540   e271b8d8           .word 0xe271b8d8
80000544   2a158ac5           .word 0x2a158ac5
80000548   1f0f8040           .word 0x1f0f8040
8000054c   dfef7738           .word 0xdfef7738
80000550   bfbfbfdf           .word 0xbfbfbfdf
80000554   dfbfbfbe           .word 0xdfbfbfbe
80000558   3877efdf           .word 0x3877efdf
8000055c   a0408f1f           .word 0xa0408f1f
80000560   00010001           .word 0x00010001
80000564   00000000           .word 0x00000000
80000568   01010101           .word 0x01010101
8000056c   01010101           .word 0x01010101
80000570   00000000           .word 0x00000000
80000574   00010001           .word 0x00010001
80000578   c2850a15           .word 0xc2850a15
8000057c   d8b870e1           .word 0xd8b870e1
80000580   ecececdc           .word 0xecececdc
80000584   5cececec           .word 0x5cececec
80000588   e2313858           .word 0xe2313858
8000058c   2a158ac5           .word 0x2a158ac5
80000590   1f0f8040           .word 0x1f0f8040
80000594   dfef7738           .word 0xdfef7738
80000598   bfbfbfdf           .word 0xbfbfbfdf
8000059c   debebcbd           .word 0xdebebcbd
800005a0   3877efdf           .word 0x3877efdf
800005a4   a0408f1f           .word 0xa0408f1f
800005a8   00010001           .word 0x00010001
800005ac   00000000           .word 0x00000000
800005b0   01010101           .word 0x01010101
800005b4   01010101           .word 0x01010101
800005b8   00000000           .word 0x00000000
800005bc   00010001           .word 0x00010001
800005c0   c2850a15           .word 0xc2850a15
800005c4   d8b870e1           .word 0xd8b870e1
800005c8   ecececdc           .word 0xecececdc
800005cc   dcececec           .word 0xdcececec
800005d0   e271b8d8           .word 0xe271b8d8
800005d4   2a158ac5           .word 0x2a158ac5
800005d8   1f0f8040           .word 0x1f0f8040
800005dc   dfef7738           .word 0xdfef7738
800005e0   bfbfbfdf           .word 0xbfbfbfdf
800005e4   ddbdbdbd           .word 0xddbdbdbd
800005e8   3875eddd           .word 0x3875eddd
800005ec   a0408f1d           .word 0xa0408f1d
800005f0   00010001           .word 0x00010001
800005f4   00000000           .word 0x00000000
800005f8   01010101           .word 0x01010101
800005fc   01010101           .word 0x01010101
80000600   00000000           .word 0x00000000
80000604   00010001           .word 0x00010001
80000608   c2850a15           .word 0xc2850a15
8000060c   d8b870e1           .word 0xd8b870e1
80000610   ecececdc           .word 0xecececdc
80000614   dcececec           .word 0xdcececec
80000618   e271b8d8           .word 0xe271b8d8
8000061c   2a158ac5           .word 0x2a158ac5
80000620   1f0f8040           .word 0x1f0f8040
80000624   dfef7738           .word 0xdfef7738
80000628   bfbfbfdf           .word 0xbfbfbfdf
8000062c   d3bbb9bd           .word 0xd3bbb9bd
80000630   3867e7d7           .word 0x3867e7d7
80000634   a0408f1f           .word 0xa0408f1f
80000638   00010001           .word 0x00010001
8000063c   00000000           .word 0x00000000
80000640   01010101           .word 0x01010101
80000644   01010101           .word 0x01010101
80000648   00000000           .word 0x00000000
8000064c   00010001           .word 0x00010001
80000650   c2850a15           .word 0xc2850a15
80000654   d8b870e1           .word 0xd8b870e1
80000658   ecececdc           .word 0xecececdc
8000065c   dcececec           .word 0xdcececec
80000660   e271b8d8           .word 0xe271b8d8
80000664   2a158ac5           .word 0x2a158ac5
80000668   1f0f8040           .word 0x1f0f8040
8000066c   dfef7738           .word 0xdfef7738
80000670   bfbfbfdf           .word 0xbfbfbfdf
80000674   9f8fa7b3           .word 0x9f8fa7b3
80000678   3877efdf           .word 0x3877efdf
8000067c   a0408f1f           .word 0xa0408f1f
80000680   00010001           .word 0x00010001
80000684   00000000           .word 0x00000000
80000688   01010101           .word 0x01010101
8000068c   01010101           .word 0x01010101
80000690   00000000           .word 0x00000000
80000694   00010001           .word 0x00010001
80000698            _Fx_DLY_TapeEcho3_Coe:
80000698   00000000           .word 0x00000000
8000069c   00000000           .word 0x00000000
800006a0   3f47ae14           .word 0x3f47ae14
800006a4   3f800000           .word 0x3f800000
800006a8   00000000           .word 0x00000000
800006ac   3f800000           .word 0x3f800000
800006b0   00000000           .word 0x00000000
800006b4   3eecbd35           .word 0x3eecbd35
800006b8   00000000           .word 0x00000000
800006bc   3f800000           .word 0x3f800000
800006c0   00000000           .word 0x00000000
800006c4   3f800000           .word 0x3f800000
800006c8   3f800000           .word 0x3f800000
800006cc   00000000           .word 0x00000000
800006d0   3f7e799a           .word 0x3f7e799a
800006d4   bffe7992           .word 0xbffe7992
800006d8   3f7e799a           .word 0x3f7e799a
800006dc   3ffe7903           .word 0x3ffe7903
800006e0   bf7cf451           .word 0xbf7cf451
800006e4   3f730ced           .word 0x3f730ced
800006e8   bfafabd2           .word 0xbfafabd2
800006ec   3f0f0670           .word 0x3f0f0670
800006f0   3fafabd2           .word 0x3fafabd2
800006f4   bf02135e           .word 0xbf02135e
800006f8   3f7f8a09           .word 0x3f7f8a09
800006fc   bf7ca490           .word 0xbf7ca490
80000700   00000000           .word 0x00000000
80000704   3f7c2e99           .word 0x3f7c2e99
80000708   00000000           .word 0x00000000
8000070c   3f4f99d4           .word 0x3f4f99d4
80000710   bec1198b           .word 0xbec1198b
80000714   00000000           .word 0x00000000
80000718   3f10f2f1           .word 0x3f10f2f1
8000071c   00000000           .word 0x00000000
80000720   3e126245           .word 0x3e126245
80000724   3e926224           .word 0x3e926224
80000728   3e126245           .word 0x3e126245
8000072c   3f2d9ce0           .word 0x3f2d9ce0
80000730   be7ffc54           .word 0xbe7ffc54
80000734   3ffc8f75           .word 0x3ffc8f75
80000738   bff6dde8           .word 0xbff6dde8
8000073c   00000000           .word 0x00000000
80000740   3f749cf5           .word 0x3f749cf5
80000744   00000000           .word 0x00000000
80000748   3f7f7436           .word 0x3f7f7436
8000074c   bf7f7436           .word 0xbf7f7436
80000750   00000000           .word 0x00000000
80000754   3f7ee87d           .word 0x3f7ee87d
80000758   00000000           .word 0x00000000
8000075c   3f81592e           .word 0x3f81592e
80000760   bf761c47           .word 0xbf761c47
80000764   00000000           .word 0x00000000
80000768   3f78cea3           .word 0x3f78cea3
8000076c   00000000           .word 0x00000000
80000770   3f806905           .word 0x3f806905
80000774   bf7eff4c           .word 0xbf7eff4c
80000778   00000000           .word 0x00000000
8000077c   3f7fd157           .word 0x3f7fd157
80000780   00000000           .word 0x00000000
80000784   3fe6f556           .word 0x3fe6f556
80000788   bfdb268d           .word 0xbfdb268d
8000078c   00000000           .word 0x00000000
80000790   3f7a3ad2           .word 0x3f7a3ad2
80000794   00000000           .word 0x00000000
80000798   3f83caf3           .word 0x3f83caf3
8000079c   bff224c4           .word 0xbff224c4
800007a0   3f5e79ff           .word 0x3f5e79ff
800007a4   3ff224c4           .word 0x3ff224c4
800007a8   bf660fe4           .word 0xbf660fe4
800007ac   3f327f91           .word 0x3f327f91
800007b0   bf05dc1e           .word 0xbf05dc1e
800007b4   00000000           .word 0x00000000
800007b8   3f535c8e           .word 0x3f535c8e
800007bc   00000000           .word 0x00000000
800007c0   3ff2dcec           .word 0x3ff2dcec
800007c4   00000000           .word 0x00000000
800007c8   3ee938de           .word 0x3ee938de
800007cc   3fac4218           .word 0x3fac4218
800007d0   00000000           .word 0x00000000
800007d4   3f6ccccd           .word 0x3f6ccccd
800007d8   3fba39a4           .word 0x3fba39a4
800007dc   00000000           .word 0x00000000
800007e0   be9dddd3           .word 0xbe9dddd3
800007e4   c08bab39           .word 0xc08bab39
800007e8   00000000           .word 0x00000000
800007ec   3f800000           .word 0x3f800000
800007f0   3f800000           .word 0x3f800000
800007f4   00000000           .word 0x00000000
800007f8   3f800000           .word 0x3f800000
800007fc   00000000           .word 0x00000000
80000800   3f800000           .word 0x3f800000
80000804   3f3e76c9           .word 0x3f3e76c9
80000808   3f800000           .word 0x3f800000
8000080c   0000000d           .word 0x0000000d
80000810   00000013           .word 0x00000013
80000814   00000000           .word 0x00000000
80000818   005feae1           .word 0x005feae1
8000081c   00000000           .word 0x00000000
80000820   3f9acfd9           .word 0x3f9acfd9
80000824   bf8e1733           .word 0xbf8e1733
80000828   00000000           .word 0x00000000
8000082c   3f57ad19           .word 0x3f57ad19
80000830   00000000           .word 0x00000000
80000834   3e67ff15           .word 0x3e67ff15
80000838   3e67ff15           .word 0x3e67ff15
8000083c   00000000           .word 0x00000000
80000840   3f0c0075           .word 0x3f0c0075
80000844   00000000           .word 0x00000000
80000848   3f8e76b6           .word 0x3f8e76b6
8000084c   bf73fed8           .word 0xbf73fed8
80000850   3eada98b           .word 0x3eada98b
80000854   3f73fed8           .word 0x3f73fed8
80000858   bee78463           .word 0xbee78463
8000085c   000dfd4d           .word 0x000dfd4d
80000860   7ff202b3           .word 0x7ff202b3
80000864   000dfd4d           .word 0x000dfd4d
80000868   7ff202b3           .word 0x7ff202b3
8000086c   40000000           .word 0x40000000
80000870   40000000           .word 0x40000000
80000874   00061546           .word 0x00061546
80000878   7ff9eab8           .word 0x7ff9eab8
8000087c   0034b23c           .word 0x0034b23c
80000880   0034b23c           .word 0x0034b23c
80000884   00032462           .word 0x00032462
80000888   00000000           .word 0x00000000
8000088c   3f800000           .word 0x3f800000
80000890   3f800000           .word 0x3f800000
80000894   00000000           .word 0x00000000
80000898            picEffectTypeSlider_TapeEcho3:
80000898   a48080b1           .word 0xa48080b1
8000089c   aea0a0a4           .word 0xaea0a0a4
800008a0   aeaeaeae           .word 0xaeaeaeae
800008a4   aeaeaeae           .word 0xaeaeaeae
800008a8   aeaeaeae           .word 0xaeaeaeae
800008ac   aeaeaeae           .word 0xaeaeaeae
800008b0   aeaeaeae           .word 0xaeaeaeae
800008b4   aeaeaeae           .word 0xaeaeaeae
800008b8   aeaeaeae           .word 0xaeaeaeae
800008bc   aeaeaeae           .word 0xaeaeaeae
800008c0   b1a0aeae           .word 0xb1a0aeae
800008c4   aeaea0b1           .word 0xaeaea0b1
800008c8   a48080ae           .word 0xa48080ae
800008cc   aea0a0a4           .word 0xaea0a0a4
800008d0   aeaeaeae           .word 0xaeaeaeae
800008d4   aeaeaeae           .word 0xaeaeaeae
800008d8   aeaeaeae           .word 0xaeaeaeae
800008dc   aeaeaeae           .word 0xaeaeaeae
800008e0   aeaeaeae           .word 0xaeaeaeae
800008e4   aeaeaeae           .word 0xaeaeaeae
800008e8   aeaeaeae           .word 0xaeaeaeae
800008ec   b1a0aeae           .word 0xb1a0aeae
800008f0   aeaea0b1           .word 0xaeaea0b1
800008f4   aeaeaeae           .word 0xaeaeaeae
800008f8   a4a48080           .word 0xa4a48080
800008fc   aeaea0a0           .word 0xaeaea0a0
80000900   aeaeaeae           .word 0xaeaeaeae
80000904   aeaeaeae           .word 0xaeaeaeae
80000908   aeaeaeae           .word 0xaeaeaeae
8000090c   aeaeaeae           .word 0xaeaeaeae
80000910   aeaeaeae           .word 0xaeaeaeae
80000914   aeaeaeae           .word 0xaeaeaeae
80000918   b1a0aeae           .word 0xb1a0aeae
8000091c   aeaea0b1           .word 0xaeaea0b1
80000920   aeaeaeae           .word 0xaeaeaeae
80000924   aeaeaeae           .word 0xaeaeaeae
80000928   a4a48080           .word 0xa4a48080
8000092c   aeaea0a0           .word 0xaeaea0a0
80000930   aeaeaeae           .word 0xaeaeaeae
80000934   aeaeaeae           .word 0xaeaeaeae
80000938   aeaeaeae           .word 0xaeaeaeae
8000093c   aeaeaeae           .word 0xaeaeaeae
80000940   aeaeaeae           .word 0xaeaeaeae
80000944   b1a0aeae           .word 0xb1a0aeae
80000948   aeaea0b1           .word 0xaeaea0b1
8000094c   aeaeaeae           .word 0xaeaeaeae
80000950   aeaeaeae           .word 0xaeaeaeae
80000954   80aeaeae           .word 0x80aeaeae
80000958   a0a4a480           .word 0xa0a4a480
8000095c   aeaeaea0           .word 0xaeaeaea0
80000960   aeaeaeae           .word 0xaeaeaeae
80000964   aeaeaeae           .word 0xaeaeaeae
80000968   aeaeaeae           .word 0xaeaeaeae
8000096c   aeaeaeae           .word 0xaeaeaeae
80000970   b1a0aeae           .word 0xb1a0aeae
80000974   aeaea0b1           .word 0xaeaea0b1
80000978   aeaeaeae           .word 0xaeaeaeae
8000097c   aeaeaeae           .word 0xaeaeaeae
80000980   aeaeaeae           .word 0xaeaeaeae
80000984   80aeaeae           .word 0x80aeaeae
80000988   a0a4a480           .word 0xa0a4a480
8000098c   aeaeaea0           .word 0xaeaeaea0
80000990   aeaeaeae           .word 0xaeaeaeae
80000994   aeaeaeae           .word 0xaeaeaeae
80000998   aeaeaeae           .word 0xaeaeaeae
8000099c   b1a0aeae           .word 0xb1a0aeae
800009a0   aeaea0b1           .word 0xaeaea0b1
800009a4   aeaeaeae           .word 0xaeaeaeae
800009a8   aeaeaeae           .word 0xaeaeaeae
800009ac   aeaeaeae           .word 0xaeaeaeae
800009b0   aeaeaeae           .word 0xaeaeaeae
800009b4   8080aeae           .word 0x8080aeae
800009b8   a0a0a4a4           .word 0xa0a0a4a4
800009bc   aeaeaeae           .word 0xaeaeaeae
800009c0   aeaeaeae           .word 0xaeaeaeae
800009c4   aeaeaeae           .word 0xaeaeaeae
800009c8   b1a0aeae           .word 0xb1a0aeae
800009cc   aeaea0b1           .word 0xaeaea0b1
800009d0   aeaeaeae           .word 0xaeaeaeae
800009d4   aeaeaeae           .word 0xaeaeaeae
800009d8   aeaeaeae           .word 0xaeaeaeae
800009dc   aeaeaeae           .word 0xaeaeaeae
800009e0   aeaeaeae           .word 0xaeaeaeae
800009e4   8080aeae           .word 0x8080aeae
800009e8   a0a0a4a4           .word 0xa0a0a4a4
800009ec   aeaeaeae           .word 0xaeaeaeae
800009f0   aeaeaeae           .word 0xaeaeaeae
800009f4   b1a0aeae           .word 0xb1a0aeae
800009f8   aeaea0b1           .word 0xaeaea0b1
800009fc   aeaeaeae           .word 0xaeaeaeae
80000a00   aeaeaeae           .word 0xaeaeaeae
80000a04   aeaeaeae           .word 0xaeaeaeae
80000a08   aeaeaeae           .word 0xaeaeaeae
80000a0c   aeaeaeae           .word 0xaeaeaeae
80000a10   aeaeaeae           .word 0xaeaeaeae
80000a14   a48080ae           .word 0xa48080ae
80000a18   aea0a0a4           .word 0xaea0a0a4
80000a1c   aeaeaeae           .word 0xaeaeaeae
80000a20   b1a0aeae           .word 0xb1a0aeae
80000a24   aeaea0b1           .word 0xaeaea0b1
80000a28   aeaeaeae           .word 0xaeaeaeae
80000a2c   aeaeaeae           .word 0xaeaeaeae
80000a30   aeaeaeae           .word 0xaeaeaeae
80000a34   aeaeaeae           .word 0xaeaeaeae
80000a38   aeaeaeae           .word 0xaeaeaeae
80000a3c   aeaeaeae           .word 0xaeaeaeae
80000a40   aeaeaeae           .word 0xaeaeaeae
80000a44   a48080ae           .word 0xa48080ae
80000a48   aea0a0a4           .word 0xaea0a0a4
80000a4c   b1a0aeae           .word 0xb1a0aeae
80000a50   aeaea0b1           .word 0xaeaea0b1
80000a54   aeaeaeae           .word 0xaeaeaeae
80000a58   aeaeaeae           .word 0xaeaeaeae
80000a5c   aeaeaeae           .word 0xaeaeaeae
80000a60   aeaeaeae           .word 0xaeaeaeae
80000a64   aeaeaeae           .word 0xaeaeaeae
80000a68   aeaeaeae           .word 0xaeaeaeae
80000a6c   aeaeaeae           .word 0xaeaeaeae
80000a70   aeaeaeae           .word 0xaeaeaeae
80000a74   a4a48080           .word 0xa4a48080
80000a78   b1a0a0a0           .word 0xb1a0a0a0
80000a7c   00000000           .word 0x00000000
80000a80            TapeEcho3:
80000a80   664f6e4f           .word 0x664f6e4f
80000a84   00000066           .word 0x00000066
80000a88   00000000           .word 0x00000000
80000a8c   00000001           .word 0x00000001
80000a90   00000000           .word 0x00000000
80000a94   00000000           .word 0x00000000
80000a98   00000000           .word 0x00000000
80000a9c   00000988           .word 0x00000988
80000aa0   00000000           .word 0x00000000
80000aa4   00000000           .word 0x00000000
80000aa8   00000000           .word 0x00000000
80000aac   00000000           .word 0x00000000
80000ab0   65706154           .word 0x65706154
80000ab4   6f686345           .word 0x6f686345
80000ab8   00000033           .word 0x00000033
80000abc   ffffffff           .word 0xffffffff
80000ac0   00000000           .word 0x00000000
80000ac4   00000001           .word 0x00000001
80000ac8   00000000           .word 0x00000000
80000acc   00000d18           .word 0x00000d18
80000ad0   00001620           .word 0x00001620
80000ad4   00000000           .word 0x00000000
80000ad8   422c0000           .word 0x422c0000
80000adc   00000000           .word 0x00000000
80000ae0   00422e46           .word 0x00422e46
80000ae4   00000000           .word 0x00000000
80000ae8   00000000           .word 0x00000000
80000aec   00000064           .word 0x00000064
80000af0   00000014           .word 0x00000014
80000af4   00000000           .word 0x00000000
80000af8   00000000           .word 0x00000000
80000afc   000017c0           .word 0x000017c0
80000b00   00000000           .word 0x00000000
80000b04   00000000           .word 0x00000000
80000b08   00000000           .word 0x00000000
80000b0c   00000000           .word 0x00000000
80000b10   0058494d           .word 0x0058494d
80000b14   00000000           .word 0x00000000
80000b18   00000000           .word 0x00000000
80000b1c   00000064           .word 0x00000064
80000b20   00000032           .word 0x00000032
80000b24   00000000           .word 0x00000000
80000b28   00000000           .word 0x00000000
80000b2c   00000a54           .word 0x00000a54
80000b30   00000000           .word 0x00000000
80000b34   00000000           .word 0x00000000
80000b38   00000000           .word 0x00000000
80000b3c   00000000           .word 0x00000000
80000b40   454d4954           .word 0x454d4954
80000b44   00000000           .word 0x00000000
80000b48   00000000           .word 0x00000000
80000b4c   000003de           .word 0x000003de
80000b50   0000015e           .word 0x0000015e
80000b54   00000000           .word 0x00000000
80000b58   00000000           .word 0x00000000
80000b5c   000007e0           .word 0x000007e0
80000b60   00000000           .word 0x00000000
80000b64   00001280           .word 0x00001280
80000b68   00000000           .word 0x00000000
80000b6c   00000000           .word 0x00000000
80000b70   4c636552           .word 0x4c636552
80000b74   00000076           .word 0x00000076
80000b78   00000000           .word 0x00000000
80000b7c   00000064           .word 0x00000064
80000b80   00000032           .word 0x00000032
80000b84   00000000           .word 0x00000000
80000b88   00000000           .word 0x00000000
80000b8c   00000b20           .word 0x00000b20
80000b90   00000000           .word 0x00000000
80000b94   00000000           .word 0x00000000
80000b98   00000000           .word 0x00000000
80000b9c   00000000           .word 0x00000000
80000ba0   434e5953           .word 0x434e5953
80000ba4   00000000           .word 0x00000000
80000ba8   00000000           .word 0x00000000
80000bac   0000000f           .word 0x0000000f
80000bb0   00000000           .word 0x00000000
80000bb4   00000000           .word 0x00000000
80000bb8   00000000           .word 0x00000000
80000bbc   000007e0           .word 0x000007e0
80000bc0   00000000           .word 0x00000000
80000bc4   00001386           .word 0x00001386
80000bc8   00000000           .word 0x00000000
80000bcc   00000028           .word 0x00000028
80000bd0   6d412d50           .word 0x6d412d50
80000bd4   00000070           .word 0x00000070
80000bd8   00000000           .word 0x00000000
80000bdc   00000001           .word 0x00000001
80000be0   00000000           .word 0x00000000
80000be4   00000000           .word 0x00000000
80000be8   00000000           .word 0x00000000
80000bec   000017a0           .word 0x000017a0
80000bf0   00000000           .word 0x00000000
80000bf4   000013d8           .word 0x000013d8
80000bf8   00000000           .word 0x00000000
80000bfc   00000004           .word 0x00000004
80000c00            effectTypeImageInfo:
80000c00   00000000           .word 0x00000000
80000c04   00000001           .word 0x00000001
80000c08   00000000           .word 0x00000000
80000c0c   00000080           .word 0x00000080
80000c10   00000040           .word 0x00000040
80000c14   80000000           .word 0x80000000
80000c18   0000001a           .word 0x0000001a
80000c1c   00000019           .word 0x00000019
80000c20   00000003           .word 0x00000003
80000c24   00000002           .word 0x00000002
80000c28   00000018           .word 0x00000018
80000c2c   00000024           .word 0x00000024
80000c30   80000e0c           .word 0x80000e0c
80000c34   00000003           .word 0x00000003
80000c38   00000032           .word 0x00000032
80000c3c   00000024           .word 0x00000024
80000c40   80000e0c           .word 0x80000e0c
80000c44   00000004           .word 0x00000004
80000c48   0000004b           .word 0x0000004b
80000c4c   0000001b           .word 0x0000001b
80000c50   80000e24           .word 0x80000e24
80000c54   00000000           .word 0x00000000
80000c58   00000000           .word 0x00000000
80000c5c   00000000           .word 0x00000000
80000c60   00000000           .word 0x00000000
80000c64   00000000           .word 0x00000000
80000c68   00000000           .word 0x00000000
80000c6c   00000000           .word 0x00000000
80000c70   00000000           .word 0x00000000
80000c74   00000000           .word 0x00000000
80000c78   00000000           .word 0x00000000
80000c7c   00000000           .word 0x00000000
80000c80   00000000           .word 0x00000000
80000c84   00000000           .word 0x00000000
80000c88   00000000           .word 0x00000000
80000c8c   00000000           .word 0x00000000
80000c90   00000000           .word 0x00000000
80000c94   00000000           .word 0x00000000
80000c98   00000000           .word 0x00000000
80000c9c   00000000           .word 0x00000000
80000ca0   00000000           .word 0x00000000
80000ca4   00000000           .word 0x00000000
80000ca8   00000000           .word 0x00000000
80000cac   00000000           .word 0x00000000
80000cb0   00000000           .word 0x00000000
80000cb4   00000000           .word 0x00000000
80000cb8   00000000           .word 0x00000000
80000cbc   00000000           .word 0x00000000
80000cc0   00000000           .word 0x00000000
80000cc4   00000000           .word 0x00000000
80000cc8   00000000           .word 0x00000000
80000ccc   00000000           .word 0x00000000
80000cd0   00000000           .word 0x00000000
80000cd4   00000000           .word 0x00000000
80000cd8            disp_prm_StompDly_BPM_sync:
80000cd8   46464f20           .word 0x46464f20
80000cdc   00001700           .word 0x00001700
80000ce0   20190000           .word 0x20190000
80000ce4   17000033           .word 0x17000033
80000ce8   0000002e           .word 0x0000002e
80000cec   00000018           .word 0x00000018
80000cf0   33201a00           .word 0x33201a00
80000cf4   2e180000           .word 0x2e180000
80000cf8   19000000           .word 0x19000000
80000cfc   00000000           .word 0x00000000
80000d00   00002e19           .word 0x00002e19
80000d04   32781900           .word 0x32781900
80000d08   78190000           .word 0x78190000
80000d0c   19000033           .word 0x19000033
80000d10   00003478           .word 0x00003478
80000d14   00357819           .word 0x00357819
80000d18   36781900           .word 0x36781900
80000d1c   78190000           .word 0x78190000
80000d20   19000037           .word 0x19000037
80000d24   00003878           .word 0x00003878
80000d28   00397819           .word 0x00397819
80000d2c   31781900           .word 0x31781900
80000d30   78190030           .word 0x78190030
80000d34   19003131           .word 0x19003131
80000d38   00323178           .word 0x00323178
80000d3c   33317819           .word 0x33317819
80000d40   31781900           .word 0x31781900
80000d44   78190034           .word 0x78190034
80000d48   19003531           .word 0x19003531
80000d4c   00363178           .word 0x00363178
80000d50   37317819           .word 0x37317819
80000d54   31781900           .word 0x31781900
80000d58   78190038           .word 0x78190038
80000d5c   19003931           .word 0x19003931
80000d60   00303278           .word 0x00303278
80000d64   00000000           .word 0x00000000
80000d68            EP_OVS_DOWN_COE:
80000d68   ba56c7ff           .word 0xba56c7ff
80000d6c   3bcd8800           .word 0x3bcd8800
80000d70   3c265100           .word 0x3c265100
80000d74   3ad73800           .word 0x3ad73800
80000d78   ba7057ff           .word 0xba7057ff
80000d7c   ba723800           .word 0xba723800
80000d80   3b7ff600           .word 0x3b7ff600
80000d84   3c30f980           .word 0x3c30f980
80000d88   3b7ff600           .word 0x3b7ff600
80000d8c   ba723800           .word 0xba723800
80000d90   ba7057ff           .word 0xba7057ff
80000d94   3ad73800           .word 0x3ad73800
80000d98   3c265100           .word 0x3c265100
80000d9c   3bcd8800           .word 0x3bcd8800
80000da0   ba56c7ff           .word 0xba56c7ff
80000da4   ba105800           .word 0xba105800
80000da8   39318001           .word 0x39318001
80000dac   3c0d2d00           .word 0x3c0d2d00
80000db0   3c0d2d00           .word 0x3c0d2d00
80000db4   39318001           .word 0x39318001
80000db8   ba105800           .word 0xba105800
80000dbc   00000000           .word 0x00000000
80000dc0            EP_OVS_UP_COE:
80000dc0   bb4a0800           .word 0xbb4a0800
80000dc4   bc1e8000           .word 0xbc1e8000
80000dc8   bcb2ba40           .word 0xbcb2ba40
80000dcc   bd229ac0           .word 0xbd229ac0
80000dd0   bd7ce1a0           .word 0xbd7ce1a0
80000dd4   bdad0730           .word 0xbdad0730
80000dd8   bdd3bc30           .word 0xbdd3bc30
80000ddc   bde9c2c0           .word 0xbde9c2c0
80000de0   bde9c2c0           .word 0xbde9c2c0
80000de4   bdd3bc30           .word 0xbdd3bc30
80000de8   bdad0730           .word 0xbdad0730
80000dec   bd7ce1a0           .word 0xbd7ce1a0
80000df0   bd229ac0           .word 0xbd229ac0
80000df4   bcb2ba40           .word 0xbcb2ba40
80000df8   bc1e8000           .word 0xbc1e8000
80000dfc   bb4a0800           .word 0xbb4a0800
80000e00            disp_prm_EP3_PreAmp_onf:
80000e00   46464f20           .word 0x46464f20
80000e04   4e4f2000           .word 0x4e4f2000
80000e08       0020           .word 0x00000020

DATA Section .fardata (Little Endian), 0x30 bytes at 0x80000e0c
80000e0c            infoEffectTypeKnob_TapeEcho3:
80000e0c   00000018           .word 0x00000018
80000e10   00000011           .word 0x00000011
80000e14   0000000b           .word 0x0000000b
80000e18   00000000           .word 0x00000000
80000e1c   0000000c           .word 0x0000000c
80000e20   80000380           .word 0x80000380
80000e24            infoEffectTypeSlider_TapeEcho3:
80000e24   0000002c           .word 0x0000002c
80000e28   00000008           .word 0x00000008
80000e2c   0000000b           .word 0x0000000b
80000e30   00000000           .word 0x00000000
80000e34   0000000c           .word 0x0000000c
80000e38   80000898           .word 0x80000898
