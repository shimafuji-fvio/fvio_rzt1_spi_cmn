/***************************************************************************
　fvIO汎用プラグイン・SSD1306専用処理ソース          作成者:シマフジ電機(株)
 ***************************************************************************/


#include "platform.h"
#include "r_port.h"
#include "r_ecl_rzt1_if.h"
#include "r_mpc2.h"
#include "r_system.h"
#include "r_icu_init.h"
#include "string.h"

#include "r_ecl_rzt1_if.h"
#include "fvIO_if.h"
#include "fvIO_cmn_if.h"
#include "fvIO_rzt1_spi_cmn_2s.h"
#include "fvIO_rzt1_spi_ssd1306.h"

static void    fvio_spi_ssd1306_init( int32_t slot_id );
static int32_t fvio_spi_ssd1306_get_inf( ST_FVIO_IF_INFO *attr );
static int32_t fvio_spi_ssd1306_trg_stop( int32_t slot_id );
static int32_t fvio_spi_ssd1306_wr_ctrl_reg( int32_t slot_id, ST_FVIO_SPI_SSD1306_CREG *attr );
static int32_t fvio_spi_ssd1306_wr_dma( int32_t slot_id, ST_FVIO_SPI_SSD1306_DMA *attr );
static int32_t fvio_spi_ssd1306_set_cnf( int32_t slot_id, ST_FVIO_SPI_SSD1306_CNF *attr );
static int32_t fvio_spi_ssd1306_get_cnf( int32_t slot_id, ST_FVIO_SPI_SSD1306_CNF *attr );
static int32_t fvio_spi_ssd1306_set_int( int32_t slot_id, ST_FVIO_SPI_SSD1306_INT *attr );
static int32_t fvio_spi_ssd1306_get_bsy( int32_t slot_id );
static int32_t fvio_spi_ssd1306_dma_restart( int32_t slot_id );
static void    fvio_spi_ssd1306_isr_pae_func( int32_t slot_id );
static void    fvio_spi_ssd1306_isr_paf_func( int32_t slot_id );
static void    fvio_spi_ssd1306_init_port( int32_t slot_id );
static void    fvio_spi_ssd1306_set_res_port( int32_t slot_id, int32_t lvl );
static void    fvio_spi_ssd1306_set_dc_port( int32_t slot_id, int32_t lvl );

typedef struct {
    ST_FVIO_SPI_SSD1306_CNF    TblfvIOCnf[FVIO_SLOT_NUM];        //config設定
    ST_FVIO_SPI_SSD1306_INT    TblfvIOInt[FVIO_SLOT_NUM];        //interrupt
    uint8_t                    TblPlugBusy[FVIO_SLOT_NUM];       //busyフラグ(DMA実行中)
}ST_FVIO_SPI_SSD1306_INFO;

ST_FVIO_SPI_SSD1306_INFO fvio_spi_ssd1306_inf;

ST_FVIO_IF_LIST fvio_spi_ssd1306_entry = {
        0x00200201,
        2,
        fvio_spi_ssd1306_assign,
        fvio_spi_ssd1306_unassign,
        fvio_spi_ssd1306_start,
        fvio_spi_ssd1306_stop,
        fvio_spi_ssd1306_write,
        fvio_spi_ssd1306_read,
        NULL,
};

/***************************************************************************
 * [名称]    :fvio_spi_ssd1306_assign
 * [機能]    :ssd1306アサイン処理
 * [引数]    :int32_t slot_id        スロットID
 *            uint32_t slot_sz       スロットサイズ
 *            void **config          プラグインデータ
 *            void *attr             実装依存の引数
 * [返値]    :int32_t                0=正常、0以外=異常
 * [備考]    :
 ***************************************************************************/
int32_t fvio_spi_ssd1306_assign( int32_t slot_id, void **config, void *attr )
{
    int32_t ret, i, cre_id = 0;

    //スロット数チェック(SSD1306)
    if( slot_id >= FVIO_SLOT_NUM ){
        return -1;
    }

    //スロットIDチェック(0,2,4,6のみOK)
    if( slot_id & 1 ){
        return -1;
    }

    //ポート初期化(hi-z)
    fvio_spi_cmn_2s_init_port_hiz(slot_id);

    //プラグインデータの登録
    *config = (void *)fvio_spi_cmn_2s_config_tbl[slot_id];

    return 0;
}

/***************************************************************************
 * [名称]    :fvio_spi_ssd1306_unassign
 * [機能]    :ssd1306アンアサイン処理
 * [引数]    :int32_t slot_id        スロットID
 * [返値]    :int32_t                0=正常、0以外=異常
 * [備考]    :
 ***************************************************************************/
int32_t fvio_spi_ssd1306_unassign( int32_t slot_id )
{
    fvio_spi_cmn_2s_init_port_hiz(slot_id);
    return 0;
}

/***************************************************************************
 * [名称]    :fvio_spi_ssd1306_start
 * [機能]    :ssd1306スタート処理
 * [引数]    :int32_t slot_id        スロットID
 *            void *attr             実装依存の引数
 * [返値]    :int32_t                0=正常、0以外=異常
 * [備考]    :
 ***************************************************************************/
int32_t fvio_spi_ssd1306_start( int32_t slot_id, void *attr )
{
    int32_t ret;
    int32_t start_id;
    volatile uint32_t i;

    //ポート初期化
    fvio_spi_cmn_2s_init_port(slot_id);
    fvio_spi_ssd1306_init_port(slot_id);

    //fvIOスタート
    start_id = (slot_id & 0xfffffffe);
    ret = R_ECL_Start( 3<<start_id, FVIO_SPI_CMN_2S_FREQ);
    if( ret != R_ECL_SUCCESS ){
        return ret;
    }

    //内部処理初期化
    fvio_spi_ssd1306_init( slot_id );
    fvio_spi_ssd1306_set_res_port( slot_id, PORT_OUTPUT_LOW );
    for( i = 0 ; i < 600 ; i++ );
    fvio_spi_ssd1306_set_res_port( slot_id, PORT_OUTPUT_HIGH );

    return 0;
}

/***************************************************************************
 * [名称]    :fvio_spi_ssd1306_stop
 * [機能]    :ssd1306ストップ処理
 * [引数]    :int32_t slot_id        スロットID
 *            void *attr             実装依存の引数
 * [返値]    :int32_t                0=正常、0以外=異常
 * [備考]    :
 ***************************************************************************/
int32_t fvio_spi_ssd1306_stop( int32_t slot_id, void *attr )
{
    int32_t stop_id;

    fvio_spi_ssd1306_trg_stop( slot_id );

    fvio_spi_cmn_2s_init_int(slot_id, 0);
    fvio_spi_cmn_2s_isr_paf[slot_id] = NULL ;    //割り込みは使用しない
    fvio_spi_cmn_2s_isr_pae[slot_id] = NULL ;    //割り込みは使用しない

    stop_id = (slot_id & 0xfffffffe);
    return R_ECL_Stop((0x03<<stop_id));
}

/***************************************************************************
 * [名称]    :fvio_spi_ssd1306_write
 * [機能]    :ssd1306ライト処理
 * [引数]    :int32_t slot_id        スロットID
 *            uint32_t mode          writeモード
 *            void *attr             実装依存の引数
 * [返値]    :int32_t                0=正常、0以外=異常
 * [備考]    :
 ***************************************************************************/
int32_t fvio_spi_ssd1306_write( int32_t slot_id, uint32_t mode, void *attr )
{
    //stop
    if( mode == FVIO_SPI_SSD1306_MODE_STOP ){
        return fvio_spi_ssd1306_trg_stop( slot_id );
    //ctrl reg
    }else if( mode == FVIO_SPI_SSD1306_MODE_CREG ){
        return fvio_spi_ssd1306_wr_ctrl_reg( slot_id, attr );
    //dma
    }else if( mode == FVIO_SPI_SSD1306_MODE_DMA ){
        return fvio_spi_ssd1306_wr_dma( slot_id, attr );
    //cnf
    }else if( mode == FVIO_SPI_SSD1306_MODE_CNF ){
        return fvio_spi_ssd1306_set_cnf( slot_id, attr );
    //int
    }else if( mode == FVIO_SPI_SSD1306_MODE_INT ){
        return fvio_spi_ssd1306_set_int( slot_id, attr );
    //dma restart
    }else if( mode == FVIO_SPI_SSD1306_MODE_RDMA ){
        return fvio_spi_ssd1306_dma_restart( slot_id );
    }else{
        return -1;
    }

    return 0;
}

/***************************************************************************
 * [名称]    :fvio_spi_ssd1306_read
 * [機能]    :ssd1306リード処理
 * [引数]    :int32_t slot_id        スロットID
 *            uint32_t mode          readモード
 *            void *attr             実装依存の引数
 * [返値]    :int32_t                0=正常、0以外=異常
 * [備考]    :
 ***************************************************************************/
int32_t fvio_spi_ssd1306_read( int32_t slot_id, uint32_t mode, void *attr )
{
    //inf
    if( mode == FVIO_SPI_SSD1306_MODE_INFO ){
        return fvio_spi_ssd1306_get_inf( attr );
    //cnf
    }else if( mode == FVIO_SPI_SSD1306_MODE_CNF ){
        return fvio_spi_ssd1306_get_cnf( slot_id, attr );
    //busy
    }else if( mode == FVIO_SPI_SSD1306_MODE_BSY ){
        return fvio_spi_ssd1306_get_bsy( slot_id );
    }else{
        return -1;
    }

    return 0;
}

/***************************************************************************
 * [名称]    :fvio_spi_ssd1306_init
 * [機能]    :ssd1306初期化処理
 * [引数]    :int32_t slot_id        スロットID
 * [返値]    :なし
 * [備考]    :
 ***************************************************************************/
static void fvio_spi_ssd1306_init( int32_t slot_id )
{
    int32_t i;

    //CONFIG設定初期化
    fvio_spi_ssd1306_inf.TblfvIOCnf[slot_id].cwait = 10;
    fvio_spi_ssd1306_inf.TblfvIOCnf[slot_id].lwait = 0;
    fvio_spi_ssd1306_inf.TblPlugBusy[slot_id]      = FVIO_SPI_CMN_2S_DMA_STOP;

    //割り込み初期化
    fvio_spi_cmn_2s_isr_paf[slot_id] = fvio_spi_ssd1306_isr_paf_func;
    fvio_spi_cmn_2s_isr_pae[slot_id] = fvio_spi_ssd1306_isr_pae_func;

    fvio_spi_cmn_2s_init_int(slot_id, 1);
}

/***************************************************************************
 * [名称]    :fvio_spi_ssd1306_get_inf
 * [機能]    :ssd1306プラグイン情報取得
 * [引数]    :ST_FVIO_IF_INFO *attr  fvioプラグイン情報
 * [返値]    :int32_t                0=正常、0以外=異常
 * [備考]    :
 ***************************************************************************/
static int32_t fvio_spi_ssd1306_get_inf( ST_FVIO_IF_INFO *attr )
{
    attr->io_type = FVIO_SPI_SSD1306_INFO_TYPE;
    attr->in_sz   = FVIO_SPI_SSD1306_INFO_INSZ;
    attr->out_sz  = FVIO_SPI_SSD1306_INFO_OUTSZ;
    return 0;
}

/***************************************************************************
 * [名称]    :fvio_spi_ssd1306_trg_stop
 * [機能]    :ssd1306 fvIOシーケンス停止
 * [引数]    :int32_t slot_id        スロットID
 * [返値]    :int32_t                0=正常、0以外=異常
 * [備考]    :
 ***************************************************************************/
static int32_t fvio_spi_ssd1306_trg_stop( int32_t slot_id )
{
    uint32_t ofst = 0x10 * slot_id ;
    uint8_t  dummy[16];

    fvio_spi_cmn_2s_stop(slot_id);                                           //TRG→STOP
    fvio_spi_cmn_2s_wait(slot_id);                                           //wait(lwait=maxなら約1[s])

    if(fvio_spi_ssd1306_inf.TblPlugBusy[slot_id] != FVIO_SPI_CMN_2S_DMA_STOP ){
        (*(volatile uint32_t*)(&DMA0.DMAC0_CHCTRL_0.LONG + ofst)) = 2;    //DMA中断（CLREN=1)
        (*(volatile uint32_t*)(&DMA0.DMAC0_CHCTRL_8.LONG + ofst)) = 2;    //DMA中断（CLREN=1)

        while( (*(volatile uint32_t*)(&DMA0.DMAC0_CHSTAT_0.LONG + ofst)) & 0x4 );
        while( (*(volatile uint32_t*)(&DMA0.DMAC0_CHSTAT_8.LONG + ofst)) & 0x4 );
        fvio_spi_cmn_2s_getfifo( slot_id, dummy, 16);                        //FIFO空読み
        fvio_spi_ssd1306_inf.TblPlugBusy[slot_id] = FVIO_SPI_CMN_2S_DMA_STOP;
    }

    return 0;
}

/***************************************************************************
 * [名称]    :fvio_spi_ssd1306_wr_ctrl_reg
 * [機能]    :ssd1306 コントロールレジスタライト
 * [引数]    :int32_t slot_id                        スロットID
 *            ST_FVIO_SPI_SSD1306_CREG *attr        コントロールレジスタのr/w用構造体
 * [返値]    :int32_t                                0=正常、0以外=異常
 * [備考]    :
 ***************************************************************************/
static int32_t fvio_spi_ssd1306_wr_ctrl_reg( int32_t slot_id, ST_FVIO_SPI_SSD1306_CREG *attr )
{
    ST_FVIO_SPI_CMN_2S_CMD para;

    //busyチェック
    if( fvio_spi_ssd1306_inf.TblPlugBusy[slot_id] != FVIO_SPI_CMN_2S_DMA_STOP ){
        return -1;
    }

    fvio_spi_ssd1306_set_dc_port( slot_id, PORT_OUTPUT_LOW );

    para.trg       = FVIO_CMN_REG_TRG_TRG;
    para.slen      = attr->sz;
    para.sdata     = attr->sdata;
    para.dma_num   = 0;
    para.cwait     = fvio_spi_ssd1306_inf.TblfvIOCnf[slot_id].cwait;
    para.lwait     = fvio_spi_ssd1306_inf.TblfvIOCnf[slot_id].lwait;
    para.opt0      = fvio_spi_ssd1306_inf.TblfvIOCnf[slot_id].sdelay;
    para.opt1      = ( FVIO_SPI_SSD1306_CLK_SED << 7 ) |
                     ( FVIO_SPI_SSD1306_CLK_CKP << 6 ) |
                     ( FVIO_SPI_SSD1306_CS_CSP << FVIO_SPI_SSD1306_CS_CH );
    para.opt2      = ( 1 << FVIO_SPI_SSD1306_CS_CH );

    if( fvio_spi_cmn_2s_cmd( slot_id, FVIO_SPI_CMN_2S_CMD_WO1, &para ) != 0 ){
        return -1;
    }

    fvio_spi_cmn_2s_wait(slot_id);

    return 0;
}

/***************************************************************************
 * [名称]    :fvio_spi_ssd1306_wr_dma
 * [機能]    :ssd1306　DMAライト
 * [引数]    :int32_t slot_id                  スロットID
 *            ST_FVIO_SPI_SSD1306_DMA *attr   DMA通信のr/w用構造体
 * [返値]    :int32_t                          0=正常、0以外=異常
 * [備考]    :
 ***************************************************************************/
static int32_t fvio_spi_ssd1306_wr_dma( int32_t slot_id, ST_FVIO_SPI_SSD1306_DMA *attr )
{
    ST_FVIO_SPI_CMN_2S_CMD para;

    //busyチェック
    if( fvio_spi_ssd1306_inf.TblPlugBusy[slot_id] != FVIO_SPI_CMN_2S_DMA_STOP ){
        return -1;
    }

    fvio_spi_ssd1306_set_dc_port( slot_id, PORT_OUTPUT_HIGH );

    para.trg       = FVIO_CMN_REG_TRG_TRG | attr->trg;
    para.slen      = attr->sz;
    para.data1     = attr->data1;
    para.data2     = attr->data2;
    para.dma_num   = attr->num;
    para.cwait     = fvio_spi_ssd1306_inf.TblfvIOCnf[slot_id].cwait;
    para.lwait     = fvio_spi_ssd1306_inf.TblfvIOCnf[slot_id].lwait;
    para.opt0      = fvio_spi_ssd1306_inf.TblfvIOCnf[slot_id].sdelay;
    para.opt1      = ( FVIO_SPI_SSD1306_CLK_SED << 7 ) |
                     ( FVIO_SPI_SSD1306_CLK_CKP << 6 ) |
                     ( FVIO_SPI_SSD1306_CS_CSP << FVIO_SPI_SSD1306_CS_CH );
    para.opt2      = ( 1 << FVIO_SPI_SSD1306_CS_CH );

    if( attr->trg & (FVIO_CMN_REG_TRG_REP | FVIO_CMN_REG_TRG_SYNC) ){
        fvio_spi_ssd1306_inf.TblPlugBusy[slot_id] = FVIO_SPI_CMN_2S_DMA_LOOP_PAF;
    }else{
        fvio_spi_ssd1306_inf.TblPlugBusy[slot_id] = FVIO_SPI_CMN_2S_DMA_BUSY_PAF;
    }

    if( fvio_spi_cmn_2s_cmd( slot_id, FVIO_SPI_CMN_2S_CMD_WO2, &para ) != 0 ){
        return -1;
    }

    return 0;
}

/***************************************************************************
 * [名称]    :fvio_spi_ssd1306_set_cnf
 * [機能]    :ssd1306 cnfデータの設定
 * [引数]    :int32_t slot_id                  スロットID
 *            ST_FVIO_SPI_SSD1306_CNF *attr   fvIO設定のr/w用構造体
 * [返値]    :int32_t                          0=正常、0以外=異常
 * [備考]    :
 ***************************************************************************/
static int32_t fvio_spi_ssd1306_set_cnf( int32_t slot_id, ST_FVIO_SPI_SSD1306_CNF *attr )
{
    fvio_spi_ssd1306_inf.TblfvIOCnf[slot_id].cwait  = attr->cwait; //分周設定
    fvio_spi_ssd1306_inf.TblfvIOCnf[slot_id].lwait  = attr->lwait; //トリガ無効期間
    fvio_spi_ssd1306_inf.TblfvIOCnf[slot_id].sdelay = attr->sdelay; //サンプリング遅延
    return 0;
}

/***************************************************************************
 * [名称]    :fvio_spi_ssd1306_get_cnf
 * [機能]    :ssd1306 cnfデータの取得
 * [引数]    :int32_t slot_id                  スロットID
 *            ST_FVIO_SPI_SSD1306_CNF *attr   fvIO設定のr/w用構造体
 * [返値]    :int32_t                          0=正常、0以外=異常
 * [備考]    :
 ***************************************************************************/
static int32_t fvio_spi_ssd1306_get_cnf( int32_t slot_id, ST_FVIO_SPI_SSD1306_CNF *attr )
{
    attr->cwait = fvio_spi_ssd1306_inf.TblfvIOCnf[slot_id].cwait;  //分周設定
    attr->lwait = fvio_spi_ssd1306_inf.TblfvIOCnf[slot_id].lwait;  //トリガ無効期間
    attr->cwait = fvio_spi_ssd1306_inf.TblfvIOCnf[slot_id].sdelay; //サンプリング遅延
    return 0;
}

/***************************************************************************
 * [名称]    :fvio_spi_ssd1306_set_int
 * [機能]    :ssd1306 割り込み関数の設定
 * [引数]    :int32_t slot_id                  スロットID
 *            ST_FVIO_SPI_SSD1306_INT *attr   ユーザー定義割り込みのr/w用構造体
 * [返値]    :int32_t                          0=正常、0以外=異常
 * [備考]    :
 ***************************************************************************/
static int32_t fvio_spi_ssd1306_set_int( int32_t slot_id, ST_FVIO_SPI_SSD1306_INT *attr )
{
    fvio_spi_ssd1306_inf.TblfvIOInt[slot_id].paf_callback = attr->paf_callback;
    fvio_spi_ssd1306_inf.TblfvIOInt[slot_id].pae_callback = attr->pae_callback;
    return 0;
}

/***************************************************************************
 * [名称]    :fvio_spi_ssd1306_get_bsy
 * [機能]    :ssd1306 busyステータスの取得
 * [引数]    :int32_t slot_id            スロットID
 * [返値]    :int32_t                    FVIO_LOOP=連続実行中、FVIO_BUSY=単発実行中、FVIO_STOP=停止
 * [備考]    :
 ***************************************************************************/
static int32_t fvio_spi_ssd1306_get_bsy( int32_t slot_id )
{
    return fvio_spi_ssd1306_inf.TblPlugBusy[slot_id];
}

/***************************************************************************
 * [名称]    :fvio_spi_ssd1306_dma_restart
 * [機能]    :ssd1306 dmaの再トリガ(paf)
 * [引数]    :int32_t slot_id           スロットID
 * [返値]    :
 * [備考]    :リピート or syncでトリガ実行した場合にDMAに対し、再トリガをかける
 ***************************************************************************/
static int32_t fvio_spi_ssd1306_dma_restart( int32_t slot_id )
{
    uint32_t ofst = 0x10 * slot_id;

    if( ( fvio_spi_ssd1306_inf.TblPlugBusy[slot_id] & FVIO_SPI_CMN_2S_DMA_LOOP_PAF ) == FVIO_SPI_CMN_2S_DMA_LOOP_PAF ){
        //次回のDMAを発行(PAF)
        (*(volatile uint32_t*)(&DMA0.DMAC0_CHCTRL_0.LONG + ofst)) = 0x8;    //SWRST=1
        (*(volatile uint32_t*)(&DMA0.DMAC0_CHCTRL_0.LONG + ofst)) = 1;      //SETEN=1
    }else{
        return -1;
    }

    return 0;
}

/***************************************************************************
 * [名称]    :fvio_spi_ssd1306_isr_pae_func
 * [機能]    :ssd1306 FIFO_PAE割り込み
 * [引数]    :int32_t slot_id           スロットID
 * [返値]    :なし
 * [備考]    :fvIO_rzt1_spi_cmn.cの各割り込み処理(fvio_spi_cmn_isr_paen)からコールされる
 ***************************************************************************/
static void fvio_spi_ssd1306_isr_pae_func( int32_t slot_id )
{
    uint32_t ofst = 0x10 * slot_id;

    //コールバック
    if( fvio_spi_ssd1306_inf.TblfvIOInt[slot_id].pae_callback != NULL ){
        fvio_spi_ssd1306_inf.TblfvIOInt[slot_id].pae_callback(slot_id);
    }

    //busyフラグクリア( LOOPの場合はクリアしない )
    if( fvio_spi_ssd1306_inf.TblPlugBusy[slot_id] & FVIO_SPI_CMN_2S_DMA_BUSY_PAE ){
        fvio_spi_ssd1306_inf.TblPlugBusy[slot_id] &= ~FVIO_SPI_CMN_2S_DMA_BUSY_PAE;
    }
}

/***************************************************************************
 * [名称]    :fvio_spi_ssd1306_isr_paf_func
 * [機能]    :ssd1306 FIFO_PAF割り込み
 * [引数]    :int32_t slot_id           スロットID
 * [返値]    :なし
 * [備考]    :fvIO_rzt1_spi_cmn.cの各割り込み処理(fvio_spi_cmn_isr_pafn)からコールされる
 ***************************************************************************/
static void fvio_spi_ssd1306_isr_paf_func( int32_t slot_id )
{
    uint32_t ofst = 0x10 * slot_id;

    //コールバック
    if( fvio_spi_ssd1306_inf.TblfvIOInt[slot_id].paf_callback != NULL ){
        fvio_spi_ssd1306_inf.TblfvIOInt[slot_id].paf_callback(slot_id);
    }

    //busyフラグクリア( LOOPの場合はクリアしない )
    if( fvio_spi_ssd1306_inf.TblPlugBusy[slot_id] & FVIO_SPI_CMN_2S_DMA_BUSY_PAF ){
        fvio_spi_ssd1306_inf.TblPlugBusy[slot_id] &= ~FVIO_SPI_CMN_2S_DMA_BUSY_PAF;
    }
}

/***************************************************************************
 * [名称]    :fvio_spi_ssd1306_init_port
 * [機能]    :ssd1306ポート初期設定
 * [引数]    :int32_t slot_id           スロットID
 * [返値]    :なし
 * [備考]    :D/CピンとRESピンをfvio制御からCPU制御に切り替える。
 *  　　　　　　　　　　fvio_spi_cmn_s2_init_port()の後で実行する。
 ***************************************************************************/
static void fvio_spi_ssd1306_init_port( int32_t slot_id )
{
    switch( slot_id )
    {
        case 0x00:
        case 0x01:
            //RES
            PORT5.PDR.BIT.B1 = PORT_DIRECTION_HIZ;
            PORT5.PMR.BIT.B1 = PORT_MODE_GENERAL;
            PORT5.PODR.BIT.B1 = PORT_OUTPUT_LOW;
            PORT5.PDR.BIT.B1 = PORT_DIRECTION_OUTPUT;
            //D/C
            PORT5.PDR.BIT.B0 = PORT_DIRECTION_HIZ;
            PORT5.PMR.BIT.B0 = PORT_MODE_GENERAL;
            PORT5.PODR.BIT.B0 = PORT_OUTPUT_LOW;
            PORT5.PDR.BIT.B0 = PORT_DIRECTION_OUTPUT;
            break;
        case 0x02:
        case 0x03:
            //RES
            PORT1.PDR.BIT.B1 = PORT_DIRECTION_HIZ;
            PORT1.PMR.BIT.B1 = PORT_MODE_GENERAL;
            PORT1.PODR.BIT.B1 = PORT_OUTPUT_LOW;
            PORT1.PDR.BIT.B1 = PORT_DIRECTION_OUTPUT;
            //D/C
            PORTT.PDR.BIT.B3 = PORT_DIRECTION_HIZ;
            PORTT.PMR.BIT.B3 = PORT_MODE_GENERAL;
            PORTT.PODR.BIT.B3 = PORT_OUTPUT_LOW;
            PORTT.PDR.BIT.B3 = PORT_DIRECTION_OUTPUT;
            break;
        case 0x04:
        case 0x05:
            //RES
            PORTA.PDR.BIT.B7 = PORT_DIRECTION_HIZ;
            PORTA.PMR.BIT.B7 = PORT_MODE_GENERAL;
            PORTA.PODR.BIT.B7 = PORT_OUTPUT_LOW;
            PORTA.PDR.BIT.B7 = PORT_DIRECTION_OUTPUT;
            //D/C
            PORTA.PDR.BIT.B6 = PORT_DIRECTION_HIZ;
            PORTA.PMR.BIT.B6 = PORT_MODE_GENERAL;
            PORTA.PODR.BIT.B6 = PORT_OUTPUT_LOW;
            PORTA.PDR.BIT.B6 = PORT_DIRECTION_OUTPUT;
            break;
        case 0x06:
        case 0x07:
            //RES
            PORT7.PDR.BIT.B2 = PORT_DIRECTION_HIZ;
            PORT7.PMR.BIT.B2 = PORT_MODE_GENERAL;
            PORT7.PODR.BIT.B2 = PORT_OUTPUT_LOW;
            PORT7.PDR.BIT.B2 = PORT_DIRECTION_OUTPUT;
            //D/C
            PORT7.PDR.BIT.B1 = PORT_DIRECTION_HIZ;
            PORT7.PMR.BIT.B1 = PORT_MODE_GENERAL;
            PORT7.PODR.BIT.B1 = PORT_OUTPUT_LOW;
            PORT7.PDR.BIT.B1 = PORT_DIRECTION_OUTPUT;
            break;
    }
}

/***************************************************************************
 * [名称]    :fvio_spi_ssd1306_set_res_port
 * [機能]    :ssd1306 RESポート出力設定
 * [引数]    :int32_t slot_id           スロットID
 * [返値]    :なし
 * [備考]    :
 ***************************************************************************/
static void fvio_spi_ssd1306_set_res_port( int32_t slot_id, int32_t lvl )
{
    switch( slot_id )
    {
        case 0x00:
        case 0x01:
            PORT5.PODR.BIT.B1 = lvl;
            break;
        case 0x02:
        case 0x03:
            PORT1.PODR.BIT.B1 = lvl;
            break;
        case 0x04:
        case 0x05:
            PORTA.PODR.BIT.B7 = lvl;
            break;
        case 0x06:
        case 0x07:
            PORT7.PODR.BIT.B2 = lvl;
            break;
    }
}

/***************************************************************************
 * [名称]    :fvio_spi_ssd1306_set_dc_port
 * [機能]    :ssd1306 D/Cポート出力設定
 * [引数]    :int32_t slot_id           スロットID
 * [返値]    :なし
 * [備考]    :
 ***************************************************************************/
static void fvio_spi_ssd1306_set_dc_port( int32_t slot_id, int32_t lvl )
{
    switch( slot_id )
    {
        case 0x00:
        case 0x01:
            PORT5.PODR.BIT.B0 = lvl;
            break;
        case 0x02:
        case 0x03:
            PORTT.PODR.BIT.B3 = lvl;
            break;
        case 0x04:
        case 0x05:
            PORTA.PODR.BIT.B6 = lvl;
            break;
        case 0x06:
        case 0x07:
            PORT7.PODR.BIT.B1 = lvl;
            break;
    }
}

