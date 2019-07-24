/***************************************************************************
メインサンプル                                       作成者:シマフジ電機(株)
 ***************************************************************************/

#include "platform.h"
#include "iodefine.h"
#include "r_system.h"
#include "r_icu_init.h"
#include "r_mpc2.h"
#include "r_port.h"
#include "r_ecm.h"

#include "r_ecl_rzt1_if.h"
#include "r_reset2.h"
#include "fvIO_cmn_if.h"
#include "fvIO_cmn_2s_if.h"
#include "fvIO_if.h"
#include "string.h"

#include "fvIO_rzt1_spi_ssd1306.h"
#include "fvIO_rzt1_spi_mpu6500.h"
#include "utility.h"

#define SLOT_NO_SSD     (0)
#define SLOT_NO_MPU     (2)

int main(void);

void user_mpu_isr( int32_t slot_id );
void user_ssd_isr( int32_t slot_id );

uint8_t oled_startup_cmd[][4] = {
         //adr,data,data,size
          0xAE,0x00,0x00,0,        //display off
          0xA8,0x3F,0x00,1,        //Set Multiplex Ratio
          0xD3,0x00,0x00,1,        //display offset
          0x40,0x00,0x00,0,        //set display start line
          0xA0,0x00,0x00,0,        //Set Segment Re-map
          0xC0,0x00,0x00,0,        //Set COM Output Scan Direction
          0xDA,0x11,0x00,1,        //Set COM Pins Hardware Configuration
          0x81,0x7F,0x00,1,        //Set Contrast Control
          0xA4,0x00,0x00,0,        //Entire Display ON
          0xA6,0x00,0x00,0,        //Set Normal/Inverse Display
          0xD5,0x80,0x00,1,        //Set Display Clock Divide Ratio/Oscillator Frequency
          0x20,0x01,0x00,1,        //Set Memory Addressing Mode
          0x21,0x00,0x7F,2,        //Set Column Address
          0x22,0x00,0x07,2,        //Set Page Address
          0x8D,0x14,0x00,1,        //Charge Pump Setting
          0xAF,0x00,0x00,0,        //display on
          0xff,0xff,0xff,0xff,     //end(size=0)
};

uint8_t mpu_startup_cmd[][3] = {
        //adr,                             data,size
        FVIO_SPI_MPU6500_REG_SMPLRT_DIV   ,0x00,0x01,
        FVIO_SPI_MPU6500_REG_CONFIG       ,0x00,0x01,
        FVIO_SPI_MPU6500_REG_GYRO_CONFIG  ,0x00,0x01,
        FVIO_SPI_MPU6500_REG_ACCEL_CONFIG ,0x00,0x01,
        FVIO_SPI_MPU6500_REG_ACCEL_CONFIG2,0x06,0x01,
        FVIO_SPI_MPU6500_REG_USER_CTRL    ,0x00,0x01,
        FVIO_SPI_MPU6500_REG_PWR_MGMT_1   ,0x01,0x01,
        FVIO_SPI_MPU6500_REG_PWR_MGMT_2   ,0x00,0x01,
        0xff                              ,0xff,0xff,
};

ST_FVIO_SPI_SSD1306_PACKET oled_data[2][128*2] __attribute__ ((section(".uncached_section")));    //OLED出力値
uint8_t acc_data[2][128*7] __attribute__ ((section(".uncached_section")));                    //acc取得値

int32_t wend=0;
int32_t rend=0;
int8_t r_p=0;                                //ACCから入力中のバッファを示す
int8_t w_p=0;                                //OLEDへ出力中のバッファを示す

int32_t plug_id_ssd, plug_id_mpu;

/***************************************************************************
 * [名称]    :icu_init
 * [機能]    :割り込み初期化
 * [引数]    :なし
 * [返値]    :なし
 * [備考]    :
 ***************************************************************************/
void icu_init(void)
{
    /* Initialize VIC (dummy writing to HVA0 register) */
    HVA0_DUMMY_WRITE();

    /* Enable IRQ interrupt (Clear CPSR.I bit to 0) */
    asm("cpsie i");   // Clear CPSR.I bit to 0
    asm("isb");       // Ensuring Context-changing
}

/***************************************************************************
 * [名称]    :user_adxl_isr
 * [機能]    :adxl355とのDMA完了割り込み(FIFO_PAE)
 * [引数]    :なし
 * [返値]    :なし
 * [備考]    :
 ***************************************************************************/
void user_mpu_isr( int32_t slot_id )
{
    //DMA再実行
    if( slot_id == SLOT_NO_MPU ){
        fvio_write( plug_id_mpu, FVIO_SPI_MPU6500_MODE_RDMA, NULL );
        r_p = (~r_p)&1;
        rend=1;
    }
}

/***************************************************************************
 * [名称]    :user_ssd_isr
 * [機能]    :ssd1306とのDMA完了割り込み(FIFO_PAF)
 * [引数]    :なし
 * [返値]    :なし
 * [備考]    :
 ***************************************************************************/
void user_ssd_isr( int32_t slot_id )
{
    if( slot_id == SLOT_NO_SSD ){
        w_p = (~w_p)&1;
        wend=1;
    }
}

/*******************************************************************************
 * [名称]    :main
 * [機能]    :メイン処理
 * [引数]    :なし
 * [返値]    :なし
 * [備考]    :
*******************************************************************************/
int main (void)
{
    int32_t i;
    int32_t dev_id_ssd, dev_id_mpu;
    uint8_t oled_col[8];
    uint8_t sdata[8], rddata[8];
    int32_t result[FVIO_SLOT_NUM]={0,0,0,0,0,0,0,0};
    int8_t r_wkp, w_wkp;

    ST_FVIO_SPI_MPU6500_CNF   mpu_cnf;
    ST_FVIO_SPI_MPU6500_CREG  mpu_creg;
    ST_FVIO_SPI_MPU6500_DMA   mpu_dma;
    ST_FVIO_SPI_MPU6500_INT   mpu_int;

    ST_FVIO_SPI_SSD1306_CNF   ssd_cnf;
    ST_FVIO_SPI_SSD1306_CREG  ssd_creg;
    ST_FVIO_SPI_SSD1306_DMA   ssd_dma;
    ST_FVIO_SPI_SSD1306_INT   ssd_int;

    //ダミーの待ち時間
    for (i = 0; i < 10000000 ; i++ )
    {
        asm("nop");
    }
    
    icu_init();

    //fvio初期化
    fvio_sys_init();

    //fvioエントリ
    if( fvio_entry( &fvio_spi_mpu6500_entry, 0, &dev_id_mpu ) < 0 ){
        goto end;
    }

    if( fvio_entry( &fvio_spi_ssd1306_entry, 0, &dev_id_ssd ) < 0 ){
        goto end;
    }

    //fvIOアサイン
    if( fvio_assign(dev_id_mpu, SLOT_NO_MPU, &plug_id_mpu, NULL ) < 0 ){
        goto end;
    }

    if( fvio_assign(dev_id_ssd, SLOT_NO_SSD, &plug_id_ssd, NULL ) < 0 ){
        goto end;
    }

    //fvIOスタート
    if( fvio_sys_start(result, NULL) != 0x0 ){
        goto end;
    }

    //fvIOプラグイン設定
    ssd_cnf.lwait=0;
    ssd_cnf.cwait=3;
    ssd_cnf.sdelay=0;
    fvio_write(plug_id_ssd, FVIO_SPI_SSD1306_MODE_CNF, &ssd_cnf);

    mpu_cnf.lwait=0;
    mpu_cnf.cwait=2;
    mpu_cnf.sdelay=0;
    fvio_write(plug_id_mpu, FVIO_SPI_MPU6500_MODE_CNF, &mpu_cnf);

    //SSD1306のスタートアップ設定
    for( i = 0 ; oled_startup_cmd[i][3] != 0xff ; i++ ){
        ssd_creg.trg = 0;
        sdata[0]   = oled_startup_cmd[i][0];
        sdata[1]   = oled_startup_cmd[i][1];
        sdata[2]   = oled_startup_cmd[i][2];
        ssd_creg.sdata  = sdata;
        ssd_creg.sz     = oled_startup_cmd[i][3];
        fvio_write( plug_id_ssd, FVIO_SPI_SSD1306_MODE_CREG, &ssd_creg );
    }

    for( i = 0 ; mpu_startup_cmd[i][2] != 0xff ; i++ ){
        mpu_creg.trg = 0;
        sdata[0]   = mpu_startup_cmd[i][0];
        sdata[1]   = mpu_startup_cmd[i][1];
        mpu_creg.sdata = sdata;
        mpu_creg.sz = mpu_startup_cmd[i][2];
        fvio_write( plug_id_mpu, FVIO_SPI_MPU6500_MODE_CREG, &mpu_creg );
    }

    //割り込みハンドラ設定
    mpu_int.pae_callback = user_mpu_isr;
    mpu_int.paf_callback = NULL;
    fvio_write( plug_id_mpu, FVIO_SPI_MPU6500_MODE_INT, &mpu_int );

    ssd_int.pae_callback = NULL;
    ssd_int.paf_callback = user_ssd_isr;
    fvio_write( plug_id_ssd, FVIO_SPI_SSD1306_MODE_INT, &ssd_int );

    //DMA転送開始(SSD1306)
    ssd_dma.trg   = FVIO_CMN_2S_REG_TRG_REP;
    ssd_dma.data1 = (uint8_t*)oled_data[0];
    ssd_dma.data2 = (uint8_t*)oled_data[1];
    ssd_dma.sz    = 3;
    ssd_dma.num   = 128*2;
    fvio_write( plug_id_ssd, FVIO_SPI_SSD1306_MODE_DMA, &ssd_dma );

    //DMA転送開始(MPU6500)
    mpu_dma.trg   = FVIO_CMN_REG_TRG_REP;
    mpu_dma.data1 = acc_data[0];
    mpu_dma.data2 = acc_data[1];
    mpu_dma.sz    = 6;
    mpu_dma.num   = 128;
    fvio_write( plug_id_mpu, FVIO_SPI_MPU6500_MODE_DMA, &mpu_dma );

    while(1){
        //受信完了
        if( wend == 1 && rend == 1 ){
            wend = 0;
            rend = 0;
            r_wkp = (~r_p)&1;
            w_wkp = (~w_p)&1;

            //DMA再実行
            fvio_write( plug_id_ssd, FVIO_SPI_SSD1306_MODE_RDMA, NULL );

            for( i = 0 ; i < 128 ; i++ ){
                //データ変換(1列分)
                conv_adxl2ssd( &acc_data[r_wkp][(i*7)+1], oled_col, 1 );

                //画面プラス領域
                oled_data[w_wkp][i*2].data[0] = oled_col[0];
                oled_data[w_wkp][i*2].data[1] = oled_col[1];
                oled_data[w_wkp][i*2].data[2] = oled_col[2];
                oled_data[w_wkp][i*2].data[3] = oled_col[3];

                //画面マイナス領域
                oled_data[w_wkp][i*2+1].data[0] = oled_col[4];
                oled_data[w_wkp][i*2+1].data[1] = oled_col[5];
                oled_data[w_wkp][i*2+1].data[2] = oled_col[6];
                oled_data[w_wkp][i*2+1].data[3] = oled_col[7];
            }
        }
    }
end:
    return 0;
}
/*******************************************************************************
 End of function main
*******************************************************************************/

/* End of File */
