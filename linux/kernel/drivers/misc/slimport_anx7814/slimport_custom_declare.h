#include "slimport.h"
#include "slimport_tx_drv.h"
#ifdef CEC_ENABLE
#include "slimport_tx_cec.h"
#endif

/*#define CO2_CABLE_DET_MODE*/
/*//#define CHANGE_TX_P0_ADDR*/

//#define SP_REGISTER_SET_TEST

#ifdef SP_REGISTER_SET_TEST
#define sp_tx_link_phy_initialization() do{ \
	pr_info("sp_tx_link_phy_initialization+, with SP_REGISTER_SET_TEST enabled !!\n"); \
	pr_info("REG0 = 0x%x\n", val_SP_TX_LT_CTRL_REG0); \
	pr_info("REG10 = 0x%x\n", val_SP_TX_LT_CTRL_REG10); \
	pr_info("REG1 = 0x%x\n", val_SP_TX_LT_CTRL_REG1); \
	pr_info("REG11 = 0x%x\n", val_SP_TX_LT_CTRL_REG11); \
	pr_info("REG2 = 0x%x\n", val_SP_TX_LT_CTRL_REG2); \
	pr_info("REG12 = 0x%x\n", val_SP_TX_LT_CTRL_REG12); \
	pr_info("REG3 = 0x%x\n", val_SP_TX_LT_CTRL_REG3); \
	pr_info("REG13 = 0x%x\n", val_SP_TX_LT_CTRL_REG13); \
	pr_info("REG4 = 0x%x\n", val_SP_TX_LT_CTRL_REG4); \
	pr_info("REG14 = 0x%x\n", val_SP_TX_LT_CTRL_REG14); \
	pr_info("REG5 = 0x%x\n", val_SP_TX_LT_CTRL_REG5); \
	pr_info("REG15 = 0x%x\n", val_SP_TX_LT_CTRL_REG15); \
	pr_info("REG6 = 0x%x\n", val_SP_TX_LT_CTRL_REG6); \
	pr_info("REG16 = 0x%x\n", val_SP_TX_LT_CTRL_REG16); \
	pr_info("REG7 = 0x%x\n", val_SP_TX_LT_CTRL_REG7); \
	pr_info("REG17 = 0x%x\n", val_SP_TX_LT_CTRL_REG17); \
	pr_info("REG8 = 0x%x\n", val_SP_TX_LT_CTRL_REG8); \
	pr_info("REG18 = 0x%x\n", val_SP_TX_LT_CTRL_REG18); \
	pr_info("REG9 = 0x%x\n", val_SP_TX_LT_CTRL_REG9); \
	pr_info("REG19 = 0x%x\n", val_SP_TX_LT_CTRL_REG19); \
	sp_write_reg(TX_P2, SP_TX_ANALOG_CTRL0, 0x02); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG0, val_SP_TX_LT_CTRL_REG0); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG10, val_SP_TX_LT_CTRL_REG10); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG1, val_SP_TX_LT_CTRL_REG1); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG11, val_SP_TX_LT_CTRL_REG11); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG2, val_SP_TX_LT_CTRL_REG2); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG12, val_SP_TX_LT_CTRL_REG12); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG3, val_SP_TX_LT_CTRL_REG3); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG13, val_SP_TX_LT_CTRL_REG13); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG4, val_SP_TX_LT_CTRL_REG4); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG14, val_SP_TX_LT_CTRL_REG14); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG5, val_SP_TX_LT_CTRL_REG5); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG15, val_SP_TX_LT_CTRL_REG15); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG6, val_SP_TX_LT_CTRL_REG6); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG16, val_SP_TX_LT_CTRL_REG16); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG7, val_SP_TX_LT_CTRL_REG7); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG17, val_SP_TX_LT_CTRL_REG17); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG8, val_SP_TX_LT_CTRL_REG8); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG18, val_SP_TX_LT_CTRL_REG18); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG9, val_SP_TX_LT_CTRL_REG9); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG19, val_SP_TX_LT_CTRL_REG19); \
	pr_info("sp_tx_link_phy_initialization-\n"); \
	}while(0) 
#else
#define sp_tx_link_phy_initialization() do{ \
	pr_info("sp_tx_link_phy_initialization+\n"); \
	sp_write_reg(TX_P2, SP_TX_ANALOG_CTRL0, 0x02); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG0, 0x01); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG10, 0x00); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG1, 0x03); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG11, 0x00); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG2, 0x07); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG12, 0x00); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG3, 0x7f); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG13, 0x00); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG4, 0x71); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG14, 0x0c); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG5, 0x6b); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG15, 0x42); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG6, 0x7f); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG16, 0x1e); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG7, 0x73); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG17, 0x3e); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG8, 0x7f); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG18, 0x72); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG9, 0x7F); \
	sp_write_reg(TX_P1, SP_TX_LT_CTRL_REG19, 0x7e); \
	} while (0)

#endif

#ifdef CHANGE_TX_P0_ADDR
#ifdef TX_P0
	#undef TX_P0
	#define TX_P0 0x78
#endif
#endif
