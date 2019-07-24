/***************************************************************************
　fvIO汎用プラグイン・MPU6500専用処理ヘッダ          作成者:シマフジ電機(株)
 ***************************************************************************/

#ifndef _FVIO_RZT1_SPI_MPU6500_H_
#define _FVIO_RZT1_SPI_MPU6500_H_

//r/wモード
#define FVIO_SPI_MPU6500_MODE_INFO         (0)    //get info
#define FVIO_SPI_MPU6500_MODE_STOP         (1)    //stop
#define FVIO_SPI_MPU6500_MODE_CREG         (2)    //ctrl reg
#define FVIO_SPI_MPU6500_MODE_DMA          (3)    //dma access
#define FVIO_SPI_MPU6500_MODE_CNF          (4)    //config
#define FVIO_SPI_MPU6500_MODE_INT          (5)    //interrupt
#define FVIO_SPI_MPU6500_MODE_BSY          (6)    //busy
#define FVIO_SPI_MPU6500_MODE_RDMA         (7)    //dma restart

//info
#define FVIO_SPI_MPU6500_INFO_TYPE         (FVIO_IF_INFO_TYPE_IO)
#define FVIO_SPI_MPU6500_INFO_INSZ         (8)    //1パケットの最大サイズ
#define FVIO_SPI_MPU6500_INFO_OUTSZ        (8)    //1パケットの最大サイズ

//SSポート設定
#define FVIO_SPI_MPU6500_CS_CH             (0)    //チップセレクトch

//クロック・チップセレクト設定
#define FVIO_SPI_MPU6500_CLK_SED           (1)    //クロック位相(0=奇数エッジでサンプル、1=偶数エッジでサンプル)
#define FVIO_SPI_MPU6500_CLK_CKP           (1)    //クロック極性(0=正論理、1=負論理)
#define FVIO_SPI_MPU6500_CS_CSP            (1)    //チップセレクト極性(0=Hiアクティブ、1=Lowアクティブ)

//データレジスタ
#define FVIO_SPI_MPU6500_REG_XG_OFFSET_H   (0x13)
#define FVIO_SPI_MPU6500_REG_XG_OFFSET_L   (0x14)
#define FVIO_SPI_MPU6500_REG_YG_OFFSET_H   (0x15)
#define FVIO_SPI_MPU6500_REG_YG_OFFSET_L   (0x16)
#define FVIO_SPI_MPU6500_REG_ZG_OFFSET_H   (0x17)
#define FVIO_SPI_MPU6500_REG_ZG_OFFSET_L   (0x18)
#define FVIO_SPI_MPU6500_REG_SMPLRT_DIV    (0x19)
#define FVIO_SPI_MPU6500_REG_CONFIG        (0x1A)
#define FVIO_SPI_MPU6500_REG_GYRO_CONFIG   (0x1B)
#define FVIO_SPI_MPU6500_REG_ACCEL_CONFIG  (0x1C)
#define FVIO_SPI_MPU6500_REG_ACCEL_CONFIG2 (0x1D)
#define FVIO_SPI_MPU6500_REG_ACCEL_XOUT_H  (0x3B)
#define FVIO_SPI_MPU6500_REG_USER_CTRL     (0x6A)
#define FVIO_SPI_MPU6500_REG_PWR_MGMT_1    (0x6B)
#define FVIO_SPI_MPU6500_REG_PWR_MGMT_2    (0x6C)
#define FVIO_SPI_MPU6500_REG_XA_OFFSET_H   (0x77)
#define FVIO_SPI_MPU6500_REG_XA_OFFSET_L   (0x78)
#define FVIO_SPI_MPU6500_REG_YA_OFFSET_H   (0x79)
#define FVIO_SPI_MPU6500_REG_YA_OFFSET_L   (0x7A)
#define FVIO_SPI_MPU6500_REG_ZA_OFFSET_H   (0x7B)
#define FVIO_SPI_MPU6500_REG_ZA_OFFSET_L   (0x7C)

//コントロールレジスタのr/w用構造体
typedef struct {
    uint8_t trg;                           //トリガ
    uint8_t *sdata;                        //送信データ
    uint8_t *rdata;                        //受信データ
    uint8_t sz;                            //1通信当たりの長さ
}ST_FVIO_SPI_MPU6500_CREG;

//DMA通信のr/w用構造体
typedef struct {
    uint8_t  trg;                          //トリガ
    uint8_t  *data1;                       //dest adr(rbuf1)
    uint8_t  *data2;                       //dest adr(rbuf2)
    uint8_t  sz;                           //1通信当たりの長さ
    uint32_t num;                          //1回の通信回数(DMA1回=sz*num)
}ST_FVIO_SPI_MPU6500_DMA;

//fvIO設定のr/w用構造体
typedef struct {
    uint8_t  cwait;                        //分周設定
    uint32_t lwait;                        //トリガ無効期間
    uint8_t  sdelay;                       //サンプリング遅延
}ST_FVIO_SPI_MPU6500_CNF;

//ユーザー定義割り込み処理のr/w用構造体
typedef struct {
    void (*paf_callback)(int32_t);         //FIFO PAF割り込み関数
    void (*pae_callback)(int32_t);         //FIFO PAE割り込み関数
}ST_FVIO_SPI_MPU6500_INT;

int32_t fvio_spi_mpu6500_assign( int32_t slot_id, void **config, void *attr );
int32_t fvio_spi_mpu6500_unassign( int32_t slot_id );
int32_t fvio_spi_mpu6500_start( int32_t slot_id, void *attr );
int32_t fvio_spi_mpu6500_stop( int32_t slot_id, void *attr );
int32_t fvio_spi_mpu6500_write( int32_t slot_id, uint32_t mode, void *attr );
int32_t fvio_spi_mpu6500_read( int32_t slot_id, uint32_t mode, void *attr );

extern ST_FVIO_IF_LIST fvio_spi_mpu6500_entry;

#endif
