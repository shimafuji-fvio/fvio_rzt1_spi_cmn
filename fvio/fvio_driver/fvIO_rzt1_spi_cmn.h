/***************************************************************************
fvIO汎用SPIプラグイン処理ヘッダ                         作成者:シマフジ電機(株)
 ***************************************************************************/

#ifndef _FVIO_SPI_CMN_H_
#define _FVIO_SPI_CMN_H_

//fvIO freq
#define FVIO_SPI_CMN_FREQ               (9)

//fvIO CMD
#define FVIO_SPI_CMN_CMD_NUM            (4)
#define FVIO_SPI_CMN_CMD_WO1            (0)
#define FVIO_SPI_CMN_CMD_WO2            (1)
#define FVIO_SPI_CMN_CMD_RW1            (2)
#define FVIO_SPI_CMN_CMD_RW2            (3)

//fvIO 内部ステータス
#define FVIO_SPI_CMN_DMA_STOP           (0x00)    //fvIO停止
#define FVIO_SPI_CMN_DMA_BUSY_PAE       (0x01)    //DMA単発転送( fvio fifo →cpu mem )
#define FVIO_SPI_CMN_DMA_LOOP_PAE       (0x02)    //DMA連続転送( fvio fifo →cpu mem )
#define FVIO_SPI_CMN_DMA_BUSY_PAF       (0x10)    //DMA単発転送( cpu mem →fvio fifo )
#define FVIO_SPI_CMN_DMA_LOOP_PAF       (0x20)    //DMA連続転送( cpu mem →fvio fifo )

//割り込み優先度
#define FVIO_SPI_CMN_ICU_PRI            (ICU_PRIORITY_14)

typedef struct {
    uint8_t trg;
    uint8_t slen;
    uint8_t *sdata;
    uint8_t *data1;
    uint8_t *data2;
    uint32_t dma_num;
    uint8_t  cwait;
    uint32_t lwait;
    uint8_t  opt0;
    uint8_t  opt1;
    uint8_t  opt2;
} ST_FVIO_SPI_CMN_CMD;

void    fvio_spi_cmn_init_port_hiz( int32_t slot_id );
void    fvio_spi_cmn_init_port( int32_t slot_id );
int32_t fvio_spi_cmn_cmd( int32_t slot_id, uint8_t cmd, ST_FVIO_SPI_CMN_CMD *attr );
int32_t fvio_spi_cmn_stop( int32_t slot_id );
void    fvio_spi_cmn_wait( int32_t slot_id );
int32_t fvio_spi_cmn_getreg( int32_t slot_id, uint8_t *rdata, uint8_t sz);
int32_t fvio_spi_cmn_getfifo( int32_t slot_id, uint8_t *rdata, uint8_t sz);
void    fvio_spi_cmn_init_int(int32_t slot_id, uint32_t mode);

void fvio_spi_cmn_isr_pae0(void) __attribute__ ((interrupt));
void fvio_spi_cmn_isr_pae1(void) __attribute__ ((interrupt));
void fvio_spi_cmn_isr_pae2(void) __attribute__ ((interrupt));
void fvio_spi_cmn_isr_pae3(void) __attribute__ ((interrupt));
void fvio_spi_cmn_isr_pae4(void) __attribute__ ((interrupt));
void fvio_spi_cmn_isr_pae5(void) __attribute__ ((interrupt));
void fvio_spi_cmn_isr_pae6(void) __attribute__ ((interrupt));
void fvio_spi_cmn_isr_pae7(void) __attribute__ ((interrupt));

void fvio_spi_cmn_isr_paf0(void) __attribute__ ((interrupt));
void fvio_spi_cmn_isr_paf1(void) __attribute__ ((interrupt));
void fvio_spi_cmn_isr_paf2(void) __attribute__ ((interrupt));
void fvio_spi_cmn_isr_paf3(void) __attribute__ ((interrupt));
void fvio_spi_cmn_isr_paf4(void) __attribute__ ((interrupt));
void fvio_spi_cmn_isr_paf5(void) __attribute__ ((interrupt));
void fvio_spi_cmn_isr_paf6(void) __attribute__ ((interrupt));
void fvio_spi_cmn_isr_paf7(void) __attribute__ ((interrupt));

extern void (*fvio_spi_cmn_isr_pae[FVIO_SLOT_NUM])(int32_t);
extern void (*fvio_spi_cmn_isr_paf[FVIO_SLOT_NUM])(int32_t);

extern const void *fvio_spi_cmn_config_tbl[FVIO_SLOT_NUM];

#endif
