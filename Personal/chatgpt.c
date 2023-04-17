#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <log.h>

#include "schedule_ctrl.h"
#include "../base/config.h"
#include "../ble/hal/pk_tool.h"
#include "msgdispatch.h"
#include "cJSON.h"
#include "log.h"

#define TAG "schedule_ctrl"

pk_schmg_parse_result_e pk_schmg_parse_schedule(char *p_msg, int msg_len)
{
    pk_schmg_parse_result_e ret = E_SCHDL_MG_PARSE_FAIL;
    cJSON *jbody = NULL;
    cJSON *result = NULL;
    char period[20] = {0};

    pk_latest_item_t *p_latest_item = NULL;
    pk_schdl_item_t *p_schdl_item = NULL;
    pk_schedule_pack_t *p_schel_datapack = NULL;

    int i = 0;
    int j = 0;
    int pack_offset = 0;
    int pack_datalen = 0;

    if ((NULL == p_msg) || (msg_len <= 0)) {
        pkLOG_ERR("ERR:...response null\n");
        return ret;
    }

    if ((msg_len < PK_HMI_SCHEDULE_DATA_LEN_MIN) || (strlen(p_msg) < PK_HMI_SCHEDULE_DATA_LEN_MIN)) {
        pkLOG_ERR("schedule len too short\n");
        ret = E_SCHDL_MG_PARSE_EMPTY; //过短视为需要清空，不需要解析
        return ret;
    }

    jbody = cJSON_Parse(p_msg);
    if(NULL == jbody){
        pkLOG_ERR("ERR:...set json parse body\n");
        return ret;
    }

    result = cJSON_GetObjectItem(jbody, "result");
    if (NULL == result) {
        LOG_DBG("%s test line=%d\n", __func__, __LINE__); //fortest
        result = jbody; //iot
    }

    {
    LOG_INFO("********* get schedule latest start %d*********\n", E_FEED_PLAN_TYPE_MAX);
    cJSON *jlatest = NULL;
    cJSON *array_item = NULL;
    cJSON *jlatest_payload = NULL;

    jlatest = cJSON_GetObjectItem(result, "latest");
    if (NULL == jlatest) {
        LOG_DBG("%s test line=%d\n", __func__, __LINE__); //fortest
        goto parse_exit;
    }

    int latest_item_num = cJSON_GetArraySize(jlatest);
    if (latest_item_num <= 0) {
        LOG_DBG("%s test line=%d\n", __func__, __LINE__); //fortest
        LOG_INFO("get schedule latest item_num <= 0 \n");
        goto schdl_parse;
    }
    LOG_DBG("test line=%d,latest_item_num=%d\n",
            __LINE__,
            latest_item_num); //fortest

    p_schel_datapack = malloc(sizeof(pk_schedule_pack_t) + sizeof(pk_latest_item_t)*latest_item_num);
    if (NULL == p_schel_datapack) {
        LOG_ERR("malloc fail in %s, line %d\n", __func__, __LINE__);
        goto parse_exit;
    }
    p_schel_datapack->schedule_data.latest_num = 0;
    p_schel_datapack->schedule_data.sch_num = 0;

///////////////////////////////////////////////////////////
    LOG_DBG("Debug malloc size:%d in _%d\n",
        sizeof(pk_latest_item_t),
        __LINE__);
    p_latest_item = malloc(sizeof(pk_latest_item_t));
    if (NULL == p_latest_item) {
        LOG_ERR("malloc fail in %s, line %d\n", __func__, __LINE__);
        goto parse_exit;
    }

    for (i = 0; i < latest_item_num; i++) {
        array_item = cJSON_GetArrayItem(jlatest, i);
        if (array_item != NULL) {
            jlatest_payload = cJSON_GetObjectItem(array_item, "a1");
            if (jlatest_payload != NULL) {
                if(g_config->usr.app_conf.factor1 > 0){
                    p_latest_item->amount1 = jlatest_payload->valueint/(g_config->usr.app_conf.factor1);
                }else{
                    LOG_ERR("left_per_loop is %d\n", g_config->usr.app_conf.factor1);
                    p_latest_item->amount1 = jlatest_payload->valueint;
                }
                LOG_INFO("latest a1 origin:%d, per_loop:%d,amount1:%d\n",
                        jlatest_payload->valueint,
                        g_config->usr.app_conf.factor1,
                        p_latest_item->amount1);
            } else {
                LOG_DBG("%s test line=%d\n", __func__, __LINE__); //fortest
                goto parse_exit;
            }

            jlatest_payload = cJSON_GetObjectItem(array_item, "a2");
            if (jlatest_payload != NULL) {
                if(g_config->usr.app_conf.factor2 > 0){
                    p_latest_item->amount2 = jlatest_payload->valueint/(g_config->usr.app_conf.factor2);
                }else{
                    LOG_ERR("left_per_loop is %d\n", g_config->usr.app_conf.factor2);
                    p_latest_item->amount2 = jlatest_payload->valueint;
                }
                LOG_INFO("latest a2 origin:%d, per_loop:%d,amount2:%d\n",
                    jlatest_payload->valueint,
                    g_config->usr.app_conf.factor2,
                    p_latest_item->amount2);
            } else {
                LOG_DBG("%s test line=%d\n", __func__, __LINE__); //fortest
                goto parse_exit;
            }

            jlatest_payload = cJSON_GetObjectItem(array_item, "id");
            if (jlatest_payload != NULL) {
                if (strlen(jlatest_payload->valuestring) <= PK_FEED_LIST_S_D_ID_LEN) {
                    memset(p_latest_item->id, 0, PK_FEED_LIST_S_D_ID_LEN);
                    memcpy(p_latest_item->id, jlatest_payload->valuestring,
                            strlen(jlatest_payload->valuestring));
                } else {
                    LOG_DBG("%s test line=%d\n", __func__, __LINE__); //fortest
                    goto parse_exit;
                }
            } else {
                LOG_DBG("%s test line=%d\n", __func__, __LINE__); //fortest
                goto parse_exit;
            }

            jlatest_payload = cJSON_GetObjectItem(array_item,"t");
            if (jlatest_payload != NULL) {
                p_latest_item->time_s = jlatest_payload->valueint;
                
                LOG_INFO("latest_item[%d]:\n", i);
                LOG_INFO("a1 = %d\n", p_latest_item->amount1);
                LOG_INFO("a2 = %d\n", p_latest_item->amount2);
                LOG_INFO("id = %s\n", p_latest_item->id);
                LOG_INFO("t = %d\n", p_latest_item->time_s);

                memcpy(&p_schel_datapack->schedule_data.p_list_str[pack_offset], p_latest_item, sizeof(pk_latest_item_t));
                pack_offset += sizeof(pk_latest_item_t);
                LOG_INFO("pack_offset = %d\n", pack_offset);
                p_schel_datapack->schedule_data.latest_num += 1;
            } else {
                LOG_DBG("%s test line=%d\n", __func__, __LINE__); //fortest
                goto parse_exit;
            }
        } else {
            LOG_DBG("%s test line=%d\n", __func__, __LINE__); //fortest
            LOG_INFO("get schedule latest item[%d] is NULL\n", i);
            goto parse_exit;
        }
    }

    LOG_INFO("********* get schedule latest over *********\n");
    }

schdl_parse:
    if(NULL == p_schel_datapack){
        p_schel_datapack = calloc(1, sizeof(pk_schedule_pack_t));
    }

    {
    LOG_INFO("********* get schedule schdl start *********\n");
    cJSON *schedule = NULL;
    cJSON *items = NULL;
    cJSON *jrepeats = NULL;
    cJSON *array_item = NULL;
    cJSON *array_sch = NULL;
    cJSON *schedule_payload = NULL;

    schedule = cJSON_GetObjectItem(result, "schedule");
    if (schedule == NULL) {
        LOG_DBG("%s test line=%d\n", __func__, __LINE__); //fortest
        ret = E_SCHDL_MG_PARSE_FAIL;
        goto parse_exit;
    }

    char lst = 0;
    char empty_list_num = 0;
    int cur_repeats = 0;

    char sch_list_num = cJSON_GetArraySize(schedule);
    if (sch_list_num <= 0) {
        LOG_INFO("get schedule json_list_num <= 0 \n");
        ret = E_SCHDL_MG_PARSE_EMPTY; //过短视为需要清空，不需要解析
        goto parse_exit;
    }

    LOG_DBG("Debug malloc size:%d in _%d\n",
        sizeof(pk_schdl_item_t),
        __LINE__);

    p_schdl_item = malloc(sizeof(pk_schdl_item_t));
    if (NULL == p_schdl_item) {
        LOG_ERR("malloc fail in %s, line %d\n", __func__, __LINE__);
        goto parse_exit;
    }

    for(lst = 0; lst < sch_list_num; lst++)
    {
        array_sch = cJSON_GetArrayItem(schedule, lst);
        if(array_sch == NULL){
            LOG_INFO("get schedule json_arry_lst[%d] is NULL \n", lst);
            goto parse_exit;
        }

        items = cJSON_GetObjectItem(array_sch, "it");
        if (NULL == items) {
            LOG_INFO("schedule items[%d] it = null \n", lst);
            goto parse_exit;
        }

        jrepeats = cJSON_GetObjectItem(array_sch, "re");
        if (NULL == jrepeats) {
            LOG_DBG("%s test line=%d\n", __func__, __LINE__); //fortest
            goto parse_exit;
        }
        cur_repeats = 0;
        memset(period, 0, sizeof(period));
        strcpy(period, jrepeats->valuestring);
        for(j = 0; j < 20; j++)
        {
            if((period[j] < '1') || (period[j] > '7')){
                period[j] = 0;
            }else{
                period[j] -= '1';

                if(period[j] == 0){
                    period[j] = 7;      //1~7代表周日到周六
                }
                cur_repeats |= (1<<period[j]);
            }
        }
        LOG_INFO("schedule list[%d] repeats = 0x%x \n", lst, cur_repeats);

        int schdl_item_num = cJSON_GetArraySize(items);
        if (schdl_item_num <= 0) {
            empty_list_num++;
            LOG_ERR("get lst[%d] schedule json_item_num <= 0 ,num of empty lst = %d\n", lst, empty_list_num);
            if(empty_list_num < sch_list_num){
                continue;
            }else {
                ret = E_SCHDL_MG_PARSE_EMPTY; //过短视为需要清空，不需要解析
                goto parse_exit;
            }
        }

        LOG_DBG("test line=%d,schdl_item_num=%d\n",
                __LINE__,
                schdl_item_num); //fortest
        for (i = 0; i < schdl_item_num; i++) {
            array_item = cJSON_GetArrayItem(items, i);
            if (array_item != NULL) {
                schedule_payload = cJSON_GetObjectItem(array_item,"a1");
                if (schedule_payload != NULL) {
                    if(g_config->usr.app_conf.factor1 > 0){
                        p_schdl_item->amount1 = schedule_payload->valueint/(g_config->usr.app_conf.factor1);
                    }else{
                        LOG_ERR("sc left_per_loop is %d\n", g_config->usr.app_conf.factor1);
                        p_schdl_item->amount1 = schedule_payload->valueint;
                    }

                    p_schdl_item->repeats = cur_repeats;
                    LOG_INFO("a1 origin:%d, per_loop:%d,amount:%d, repeats:%d\n",
                                schedule_payload->valueint,
                                g_config->usr.app_conf.factor1,
                                p_schdl_item->amount1, 
                                p_schdl_item->repeats);
                } else {
                    LOG_DBG("%s test line=%d\n", __func__, __LINE__); //fortest
                    goto parse_exit;
                }

                schedule_payload = cJSON_GetObjectItem(array_item, "a2");
                if (schedule_payload != NULL) {
                    if(g_config->usr.app_conf.factor2 > 0){
                        p_schdl_item->amount2 = schedule_payload->valueint/(g_config->usr.app_conf.factor2);
                    }else{
                        LOG_ERR("sc right_per_loop is %d\n", g_config->usr.app_conf.factor2);
                        p_schdl_item->amount2 = schedule_payload->valueint;
                    }
                    LOG_INFO("a2 origin:%d, per_loop:%d, amount:%d, repeats:%d\n",
                                schedule_payload->valueint, 
                                g_config->usr.app_conf.factor2,
                                p_schdl_item->amount2, 
                                p_schdl_item->repeats);
                } else {
                    LOG_DBG("%s test line=%d\n", __func__, __LINE__); //fortest
                    goto parse_exit;
                }

                schedule_payload = cJSON_GetObjectItem(array_item, "id");
                if (schedule_payload != NULL) {
                    if (strlen(schedule_payload->valuestring) <= PK_FEED_LIST_N_ID_LEN) {
                        memset(p_schdl_item->id, 0, PK_FEED_LIST_N_ID_LEN);
                        memcpy(p_schdl_item->id, schedule_payload->valuestring,
                                strlen(schedule_payload->valuestring));
                    } else {
                        LOG_DBG("%s test line=%d\n", __func__, __LINE__); //fortest
                        goto parse_exit;
                    }
                } else {
                    LOG_DBG("%s test line=%d\n", __func__, __LINE__); //fortest
                    goto parse_exit;
                }

                schedule_payload = cJSON_GetObjectItem(array_item, "t");
                if (schedule_payload != NULL) {
                    p_schdl_item->time_s = schedule_payload->valueint;

                     LOG_INFO("schdl_item[%d]:\n", i);
                    LOG_INFO("a1 = %d\n", p_schdl_item->amount1);
                    LOG_INFO("a2 = %d\n", p_schdl_item->amount2);
                    LOG_INFO("id = %s\n", p_schdl_item->id);
                    LOG_INFO("t = %d\n", p_schdl_item->time_s);
                    LOG_INFO("re = 0x%x\n", p_schdl_item->repeats);

                    p_schel_datapack = realloc(p_schel_datapack, (pack_offset + sizeof(pk_schedule_pack_t) + sizeof(pk_schdl_item_t)));
                    memcpy(&p_schel_datapack->schedule_data.p_list_str[pack_offset], p_schdl_item, sizeof(pk_schdl_item_t));
                    pack_offset += sizeof(pk_schdl_item_t);
                    LOG_INFO("pack_offset = %d\n", pack_offset);
                    p_schel_datapack->schedule_data.sch_num++;
                } else {
                    LOG_DBG("%s test line=%d\n", __func__, __LINE__); //fortest
                    goto parse_exit;
                }
            } else {
                LOG_DBG("%s test line=%d\n", __func__, __LINE__); //fortest
                goto parse_exit;
            }
        }
    }

    LOG_INFO("********* get schedule schdl over *********\n");
    }

    p_schel_datapack->data_len = pack_offset + sizeof(pk_schedule_data_t);
    pack_datalen = p_schel_datapack->data_len + sizeof(p_schel_datapack->data_len);
    LOG_DBG("data_len = %d, latest_num = %d, sch_num = %d\n", p_schel_datapack->data_len, 
        p_schel_datapack->schedule_data.latest_num, p_schel_datapack->schedule_data.sch_num); //fortest

    unsigned char *data = (unsigned char *)p_schel_datapack;
    LOG_INFO("schel_datapack_dump(%d):", pack_datalen);
    for (int i = 0; i < pack_datalen; i++)
    {
        printf("%02x ", data[i]);
    };
    printf("\r\n");

    dispatch_send_msg(DISPATCH_MSG_BLE_SET_SCHEDULE, DISPATCH_RECEIVER_BLE, 
        (uint8_t *)p_schel_datapack, (p_schel_datapack->data_len + sizeof(p_schel_datapack->data_len)));
    
    ret = E_SCHDL_MG_PARSE_OK;

parse_exit:
    if (p_latest_item != NULL) {
        free(p_latest_item);
        p_latest_item = NULL;
    }

    if (p_schdl_item != NULL) {
        free(p_schdl_item);
        p_schdl_item = NULL;
    }

    if (p_schel_datapack != NULL) {
        free(p_schel_datapack);
        p_schel_datapack = NULL;
    }

    return ret;
}
