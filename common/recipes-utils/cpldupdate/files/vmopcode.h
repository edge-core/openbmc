/***************************************************************
*
* This is the include file for Lattice Semiconductor's ispVM
* Embedded software application.
*
***************************************************************/

/***************************************************************
*
* VME version.
*
* History:
*
***************************************************************/
#define VME_VERSION_NUMBER "12.2"

/***************************************************************
*
* Maximum declarations.
*
***************************************************************/

#define VMEHEXMAX   60000L  /* The hex file is split 60K per file. */
#define SCANMAX     64000L  /* The maximum SDR/SIR burst. */

/***************************************************************
*
* Supported JTAG state transitions.
*
***************************************************************/

#define RESET      0x00
#define IDLE       0x01
#define IRPAUSE    0x02
#define DRPAUSE    0x03
#define SHIFTIR    0x04
#define SHIFTDR    0x05
#define DRCAPTURE  0x06

/***************************************************************
*
* Flow control register bit definitions.  A set bit indicates
* that the register currently exhibits the corresponding mode.
*
***************************************************************/

#define INTEL_PRGM 0x0001    /* Intelligent programming is in effect. */
#define CASCADE    0x0002    /* Currently splitting large SDR. */
#define REPEATLOOP 0x0008    /* Currently executing a repeat loop. */
#define SHIFTRIGHT 0x0080    /* The next data stream needs a right shift. */
#define SHIFTLEFT  0x0100    /* The next data stream needs a left shift. */
#define VERIFYUES  0x0200    /* Continue if fail is in effect. */

/***************************************************************
*
* DataType register bit definitions.  A set bit indicates
* that the register currently holds the corresponding type of data.
*
***************************************************************/

#define EXPRESS    0x0001    /* Simultaneous program and verify. */
#define SIR_DATA   0x0002    /* SIR is the active SVF command. */
#define SDR_DATA   0x0004    /* SDR is the active SVF command. */
#define COMPRESS   0x0008    /* Data is compressed. */
#define TDI_DATA   0x0010    /* TDI data is present. */
#define TDO_DATA   0x0020    /* TDO data is present. */
#define MASK_DATA  0x0040    /* MASK data is present. */
#define HEAP_IN    0x0080    /* Data is from the heap. */
#define LHEAP_IN   0x0200    /* Data is from intel data buffer. */
#define VARIABLE   0x0400    /* Data is from a declared variable. */
#define CRC_DATA   0x0800	 /* CRC data is pressent. */
#define CMASK_DATA 0x1000    /* CMASK data is pressent. */
#define RMASK_DATA 0x2000	 /* RMASK data is pressent. */
#define READ_DATA  0x4000    /* READ data is pressent. */
#define DMASK_DATA 0x8000	 /* DMASK data is pressent. */

/***************************************************************
*
* Pin opcodes.
*
***************************************************************/

#define signalENABLE  0x1C    /* ispENABLE pin. */
#define signalTMS     0x1D    /* TMS pin. */
#define signalTCK     0x1E    /* TCK pin. */
#define signalTDI     0x1F    /* TDI pin. */
#define signalTRST    0x20    /* TRST pin. */

/***************************************************************
*
* Supported vendors.
*
***************************************************************/

#define VENDOR		0x56
#define LATTICE		0x01
#define ALTERA		0x02
#define XILINX		0x03

/***************************************************************
*
* Opcode definitions.
*
* Note: opcodes must be unique.
*
***************************************************************/

#define ENDDATA    0x00    /* The end of the current SDR data stream. */
#define RUNTEST    0x01    /* The duration to stay at the stable state. */
#define ENDDR      0x02    /* The stable state after SDR. */
#define ENDIR      0x03    /* The stable state after SIR. */
#define ENDSTATE   0x04    /* The stable state after RUNTEST. */
#define TRST       0x05    /* Assert the TRST pin. */
#define HIR        0x06    /* The sum of the IR bits of the leading devices. */
#define TIR        0x07    /* The sum of the IR bits of the trailing devices. */
#define HDR        0x08    /* The number of leading devices. */
#define TDR        0x09    /* The number of trailing devices. */
#define ispEN      0x0A    /* Assert the ispEN pin. */
#define FREQUENCY  0x0B    /* The maximum clock rate to run the JTAG state machine. */
#define STATE      0x10    /* Move to the next stable state. */
#define SIR        0x11    /* The instruction stream follows. */
#define SDR        0x12    /* The data stream follows. */
#define TDI        0x13    /* The following data stream feeds into the device. */
#define TDO        0x14    /* The following data stream is compared against the device. */
#define MASK       0x15    /* The following data stream is used as mask. */
#define XSDR       0x16    /* The following data stream is for simultaneous program and verify. */
#define XTDI       0x17    /* The following data stream is for shift in only. It must be stored for the next XSDR. */
#define XTDO       0x18    /* There is not data stream.  The data stream was stored from the previous XTDI. */
#define MEM        0x19    /* The maximum memory needed to allocate in order hold one row of data. */
#define WAIT       0x1A    /* The duration of delay to observe. */
#define TCK        0x1B    /* The number of TCK pulses. */
#define SHR        0x23    /* Set the flow control register for right shift. */
#define SHL        0x24    /* Set the flow control register for left shift. */
#define HEAP       0x32    /* The memory size needed to hold one loop. */
#define REPEAT     0x33    /* The beginning of the loop. */
#define LEFTPAREN  0x35    /* The beginning of data following the loop. */
#define VAR		     0x55	   /* Plac holder for loop data. */
#define SEC        0x1C    /* The delay time in seconds that must be observed. */
#define SMASK      0x1D    /* The mask for TDI data. */
#define MAX        0x1E    /* The absolute maximum wait time. */
#define ON         0x1F    /* Assert the targeted pin. */
#define OFF        0x20    /* Dis-assert the targeted pin. */
#define SETFLOW    0x30    /* Change the flow control register. */
#define RESETFLOW  0x31    /* Clear the flow control register. */
#define CRC		     0x47    /* The following data stream is used for CRC calculation. */
#define CMASK	     0x48	   /* The following data stream is used as mask for CRC calculation. */
#define RMASK      0x49    /* The following data stream is used as mask for read and save. */
#define READ	     0x50    /* The following data stream is used for read and save. */
#define ENDLOOP    0x59    /* The end of the repeat loop. */
#define SECUREHEAP 0x60    /* Used to secure the HEAP opcode. */
#define VUES       0x61	   /* Support continue if fail. */
#define DMASK      0x62    /* The following data stream is used for dynamic I/O. */
#define COMMENT	   0x63    /* Support SVF comments in the VME file. */
#define HEADER     0x64    /* Support header in VME file. */
#define FILE_CRC   0x65    /* Support crc-protected VME file. */
#define LCOUNT     0x66    /* Support intelligent programming. */
#define LDELAY     0x67    /* Support intelligent programming. */
#define LSDR       0x68    /* Support intelligent programming. */
#define LHEAP      0x69    /* Memory needed to hold intelligent data buffer */
#define CONTINUE   0x70    /* Allow continuation. */
#define LVDS	     0x71	   /* Support LVDS. */
#define ENDVME     0x7F    /* End of the VME file. */
#define HIGH       0x80    /* Assert the targeted pin. */
#define LOW        0x81    /* Dis-assert the targeted pin. */
#define ENDFILE    0xFF    /* End of file. */

//#define GALAXY100_I2C_PRJ 1 /* use for galaxy100 i2c project*/
#define GALAXY100_PRJ 1 /* use for galaxy100 gpio project*/
#ifdef GALAXY100_PRJ
#define I2C_CPLD_BUS 0
#define I2C_CPLD_ADDRESS 0x21
#define PCA953X_INPUT          0
#define PCA953X_OUTPUT         1
#define PCA953X_DIRECTION      3
#define GPIO_IN	1
#define GPIO_OUT 0
#define CPLD_TCK_I2C_CONFIG	0
#define CPLD_TMS_I2C_CONFIG	1
#define CPLD_TDI_I2C_CONFIG 2
#define CPLD_TDO_I2C_CONFIG	3
#define CPLD_I2C_ENABLE_OFFSET 4

/*------------分割线--------------------*/
#define GPIO_BASE_ADDR 0x1e780000

#define GPIOM_DATA_OFFSET (0x78)
#define GPIOM_DIR_OFFSET  (0x7C)

#define GPIOJ_DATA_OFFSET (0x70)
#define GPIOJ_DIR_OFFSET  (0x74)

#define GPIOS_DATA_OFFSET (0x80)
#define GPIOS_DIR_OFFSET  (0x84)
#define GPIOH_DATA_OFFSET (0x20)
#define GPIOH_DIR_OFFSET  (0x24)

#define GPIOM_DATA_ADDR (GPIO_BASE_ADDR + GPIOM_DATA_OFFSET)
#define GPIOM_DIR_ADDR (GPIO_BASE_ADDR + GPIOM_DIR_OFFSET)

#define GPIOJ_DATA_ADDR (GPIO_BASE_ADDR + GPIOJ_DATA_OFFSET)
#define GPIOJ_DIR_ADDR (GPIO_BASE_ADDR + GPIOJ_DIR_OFFSET)

#define GPIOS_DATA_ADDR (GPIO_BASE_ADDR + GPIOS_DATA_OFFSET)
#define GPIOS_DIR_ADDR (GPIO_BASE_ADDR + GPIOS_DIR_OFFSET)
#define GPIOH_DATA_ADDR (GPIO_BASE_ADDR + GPIOH_DATA_OFFSET)
#define GPIOH_DIR_ADDR (GPIO_BASE_ADDR + GPIOH_DIR_OFFSET)

//data register bit
#define BMC_GPIOM4   (4) //W3
#define BMC_GPIOM5   (5) //Y2
#define BMC_GPIOM6   (6) //AA1
#define BMC_GPIOM7   (7) //V5

#define BMC_GPIOJ4   (12)
#define BMC_GPIOJ5   (13)
#define BMC_GPIOJ6   (14)
#define BMC_GPIOJ7   (15)

#define BMC_GPIOS3   (19)
#define BMC_GPIOH0   (24)
#define BMC_GPIOH1   (25)
#define BMC_GPIOH2   (26)

/*
#define CPLD_TMS_CONFIG BMC_GPIOM4
#define CPLD_TDI_CONFIG BMC_GPIOM5
#define CPLD_TCK_CONFIG BMC_GPIOM6
#define CPLD_TDO_CONFIG BMC_GPIOM7
*/
#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)
#define SYSTEM_SCU_BASE_ADDR 0x1e6e2000
#define SYSTEM_SCU80 0x80
#define SYSTEM_SCU84 0x84
#define SYSTEM_SCU_ADDR(num) (SYSTEM_SCU_BASE_ADDR + num)

#endif
/***************************************************************
*
* ispVM Embedded Return Codes.
*
***************************************************************/

#define VME_VERIFICATION_FAILURE		-1
#define VME_FILE_READ_FAILURE			  -2
#define VME_VERSION_FAILURE				  -3
#define VME_INVALID_FILE				    -4
#define VME_ARGUMENT_FAILURE			  -5
#define VME_CRC_FAILURE					    -6

/***************************************************************
*
* Type definitions.
*
***************************************************************/

/* Support LVDS */
typedef struct {
	unsigned short usPositiveIndex;
	unsigned short usNegativeIndex;
	unsigned char  ucUpdate;
} LVDSPair;

#if defined(GALAXY100_PRJ)
extern int syscpld_update;
extern int use_dll;
extern const char *dll_name;
int isp_dll_init(int argc, const char * const argv[]);
void isp_gpio_config( unsigned int gpio, int dir, const char * cpld_type);
void isp_gpio_i2c_config( unsigned int gpio, int dir );
int isp_gpio_init(const char * cpld_type);
int isp_gpio_i2c_init(void);
void isp_gpio_uninit(const char * cpld_type);
int isp_vme_file_size_set(char *file_name);
long isp_vme_file_size_get(void);
int isp_print_progess_bar(long offset);
#endif
