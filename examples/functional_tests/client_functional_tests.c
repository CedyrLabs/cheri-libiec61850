/*
 * functional_tests.c
 *
 * This example is intended to be used with server_example_basic_io or server_example_goose.
 */

#include "iec61850_client.h"

#include <stdlib.h>
#include <stdio.h>

#include "hal_thread.h"

void printSpaces(int spaces)
{
    int i;

    for (i = 0; i < spaces; i++)
        printf(" ");
}


void printDataDirectory(char* doRef, IedConnection con, int spaces)
{
    IedClientError error;

    LinkedList dataAttributes = IedConnection_getDataDirectory(con, &error, doRef);

    //LinkedList dataAttributes = IedConnection_getDataDirectoryByFC(con, &error, doRef, MX);

    if (dataAttributes != NULL) {
        LinkedList dataAttribute = LinkedList_getNext(dataAttributes);

        while (dataAttribute != NULL) {
            char* daName = (char*) dataAttribute->data;

            printSpaces(spaces);
            printf("DA: %s\n", (char*) dataAttribute->data);

            dataAttribute = LinkedList_getNext(dataAttribute);

            char daRef[130];
            sprintf(daRef, "%s.%s", doRef, daName);
            printDataDirectory(daRef, con, spaces + 2);
        }
    }

    LinkedList_destroy(dataAttributes);
}


static void printDataSetValues(MmsValue* dataSet)
{
    int i;
    for (i = 0; i < 4; i++) {
        printf("  GGIO1.AnIn%i.mag.f: %f\n", i,
                MmsValue_toFloat(MmsValue_getElement(MmsValue_getElement(
                                            MmsValue_getElement(dataSet, i), 0), 0)));
    }
}


void reportCallbackFunction(void* parameter, ClientReport report)
{
    MmsValue* dataSetValues = ClientReport_getDataSetValues(report);

    printf("received report for %s\n", ClientReport_getRcbReference(report));

    int i;
    for (i = 0; i < 4; i++) {
        ReasonForInclusion reason = ClientReport_getReasonForInclusion(report, i);

        if (reason != IEC61850_REASON_NOT_INCLUDED) {
            printf("  GGIO1.SPCSO%i.stVal: %i (included for reason %i)\n", i,
                    MmsValue_getBoolean(MmsValue_getElement(dataSetValues, i)), reason);
        }
    }
}


void readAnalogMeasurementTests(IedConnection con, IedClientError error, const char* hostname, int tcpPort){
    printf("Begin Test = Read Analog Measurements for Server\n");
    IedConnection_connect(con, &error, hostname, tcpPort);
    /* read an analog measurement value from server */
    MmsValue* value = IedConnection_readObject(con, &error, "simpleIOGenericIO/GGIO1.AnIn1.mag.f", IEC61850_FC_MX);
    if (value != NULL) {

        if (MmsValue_getType(value) == MMS_FLOAT) {
            float fval = MmsValue_toFloat(value);
            printf("read float value: %f\n", fval);
            printf("Analog value read successful.\n");
        }
        else if (MmsValue_getType(value) == MMS_DATA_ACCESS_ERROR) {
            printf("Failed to read value (error code: %i)\n", MmsValue_getDataAccessError(value));
        }

        MmsValue_delete(value);
    }
    else{
        printf("Analog value read unsuccessful.");
    }
    IedConnection_close(con);
    printf("End Test\n");
    printf("=========================================\n");
}


void writeDataToServerTests(IedConnection con, IedClientError error, const char* hostname, int tcpPort){
    printf("Begin Test = Write Data to Server\n");
    IedConnection_connect(con, &error, hostname, tcpPort);
    MmsValue* value = MmsValue_newVisibleString("libiec61850.com");
    IedConnection_writeObject(con, &error, "simpleIOGenericIO/GGIO1.NamPlt.vendor", IEC61850_FC_DC, value);

    if (error != IED_ERROR_OK){
        printf("failed to write simpleIOGenericIO/GGIO1.NamPlt.vendor! (error code: %i)\n", error);
    }
    else{
        printf("Write Data to Server Successful.\n");
    }

    MmsValue_delete(value);
    IedConnection_close(con);
    printf("End Test\n");
    printf("=========================================\n");
}


void readDataSetTests(IedConnection con, IedClientError error, const char* hostname, int tcpPort){
    printf("Begin Test = Read Dataset\n");
    IedConnection_connect(con, &error, hostname, tcpPort);
    ClientDataSet clientDataSet = IedConnection_readDataSetValues(con, &error, "simpleIOGenericIO/LLN0.Events", NULL);

    if (clientDataSet == NULL) {
        printf("failed to read dataset\n");
    }
    else{
        printf("Dataset read successfully.");
    }
    IedConnection_close(con);
    printf("End Test\n");
    printf("=========================================\n");
}


void reportingTests(IedConnection con, IedClientError error, const char* hostname, int tcpPort){
    printf("Begin Test = Reporting\n");
    IedConnection_connect(con, &error, hostname, tcpPort);
    ClientDataSet clientDataSet = 
            IedConnection_readDataSetValues(con, &error, "simpleIOGenericIO/LLN0.Events", NULL);
    ClientReportControlBlock rcb =
            IedConnection_getRCBValues(con, &error, "simpleIOGenericIO/LLN0.RP.EventsRCB01", NULL);
    if (rcb) {
        bool rptEna = ClientReportControlBlock_getRptEna(rcb);

        printf("RptEna = %i\n", rptEna);

        /* Install handler for reports */
        IedConnection_installReportHandler(con, "simpleIOGenericIO/LLN0.RP.EventsRCB01",
                ClientReportControlBlock_getRptId(rcb), reportCallbackFunction, NULL);

        
        /* Set trigger options and enable report */
        printf("Setting triggering options.\n");
        ClientReportControlBlock_setTrgOps(rcb, TRG_OPT_DATA_UPDATE | TRG_OPT_INTEGRITY | TRG_OPT_GI);
        rptEna = ClientReportControlBlock_getRptEna(rcb);
        printf("RptEna = %i\n", rptEna);

        printf("Enabling report.\n");
        ClientReportControlBlock_setRptEna(rcb, true);
        rptEna = ClientReportControlBlock_getRptEna(rcb);
        printf("RptEna = %i\n", rptEna);

        printf("Setting integrity period to 5s.\n");
        ClientReportControlBlock_setIntgPd(rcb, 5000);
        rptEna = ClientReportControlBlock_getRptEna(rcb);
        printf("RptEna = %i\n", rptEna);

        IedConnection_setRCBValues(con, &error, rcb, RCB_ELEMENT_RPT_ENA | RCB_ELEMENT_TRG_OPS | RCB_ELEMENT_INTG_PD, true);

        if (error != IED_ERROR_OK)
            printf("Report activation failed (code: %i)\n", error);
        else
            printf("Report activation passed.\n");

        Thread_sleep(1000);

        /* trigger GI report */
        ClientReportControlBlock_setGI(rcb, true);
        rptEna = ClientReportControlBlock_getRptEna(rcb);
        printf("RptEna = %i\n", rptEna);
        printf("Triggering GI Report\n");
        IedConnection_setRCBValues(con, &error, rcb, RCB_ELEMENT_GI, true);

        if (error != IED_ERROR_OK)
            printf("Error triggering a GI report (code: %i)\n", error);
        else
            printf("RptEna = %i\n", rptEna);
            printf("Triggering GI report passed.\n");


        Thread_sleep(60000);

        /* disable reporting */
        printf("Disabling Reporting\n");
        ClientReportControlBlock_setRptEna(rcb, false);
        IedConnection_setRCBValues(con, &error, rcb, RCB_ELEMENT_RPT_ENA, true);

        if (error != IED_ERROR_OK)
            printf("disable reporting failed (code: %i)\n", error);
        else
            printf("Disabling reporting passed.\n");

        ClientDataSet_destroy(clientDataSet);

        ClientReportControlBlock_destroy(rcb);
    }
    IedConnection_close(con);
    printf("End Test\n");
    printf("=========================================\n");
}


void browseDataModel(IedConnection con, IedClientError error, const char* hostname, int tcpPort){
    printf("Begin Test = Browse Data Model in the Server...\n");
    IedConnection_connect(con, &error, hostname, tcpPort);

    if (error == IED_ERROR_OK) {

        printf("Get logical device list...\n");
        LinkedList deviceList = IedConnection_getLogicalDeviceList(con, &error);

        if (error != IED_ERROR_OK) {
            printf("Failed to read device list (error code: %i)\n", error);
            IedConnection_close(con);
        }else{
            LinkedList device = LinkedList_getNext(deviceList);

            while (device != NULL) {
                printf("LD: %s\n", (char*) device->data);

                LinkedList logicalNodes = IedConnection_getLogicalDeviceDirectory(con, &error,
                        (char*) device->data);

                LinkedList logicalNode = LinkedList_getNext(logicalNodes);

                while (logicalNode != NULL) {
                    printf("  LN: %s\n", (char*) logicalNode->data);

                    char lnRef[129];

                    sprintf(lnRef, "%s/%s", (char*) device->data, (char*) logicalNode->data);

                    LinkedList dataObjects = IedConnection_getLogicalNodeDirectory(con, &error,
                            lnRef, ACSI_CLASS_DATA_OBJECT);

                    LinkedList dataObject = LinkedList_getNext(dataObjects);

                    while (dataObject != NULL) {
                        char* dataObjectName = (char*) dataObject->data;

                        printf("    DO: %s\n", dataObjectName);

                        dataObject = LinkedList_getNext(dataObject);

                        char doRef[129];

                        sprintf(doRef, "%s/%s.%s", (char*) device->data, (char*) logicalNode->data, dataObjectName);

                        printDataDirectory(doRef, con, 6);
                    }

                    LinkedList_destroy(dataObjects);

                    LinkedList dataSets = IedConnection_getLogicalNodeDirectory(con, &error, lnRef,
                            ACSI_CLASS_DATA_SET);

                    LinkedList dataSet = LinkedList_getNext(dataSets);

                    while (dataSet != NULL) {
                        char* dataSetName = (char*) dataSet->data;
                        bool isDeletable;
                        char dataSetRef[130];
                        sprintf(dataSetRef, "%s.%s", lnRef, dataSetName);

                        LinkedList dataSetMembers = IedConnection_getDataSetDirectory(con, &error, dataSetRef,
                                &isDeletable);

                        if (isDeletable)
                            printf("    Data set: %s (deletable)\n", dataSetName);
                        else
                            printf("    Data set: %s (not deletable)\n", dataSetName);

                        LinkedList dataSetMemberRef = LinkedList_getNext(dataSetMembers);

                        while (dataSetMemberRef != NULL) {

                            char* memberRef = (char*) dataSetMemberRef->data;

                            printf("      %s\n", memberRef);

                            dataSetMemberRef = LinkedList_getNext(dataSetMemberRef);
                        }

                        LinkedList_destroy(dataSetMembers);

                        dataSet = LinkedList_getNext(dataSet);
                    }

                    LinkedList_destroy(dataSets);

                    LinkedList reports = IedConnection_getLogicalNodeDirectory(con, &error, lnRef,
                            ACSI_CLASS_URCB);

                    LinkedList report = LinkedList_getNext(reports);

                    while (report != NULL) {
                        char* reportName = (char*) report->data;

                        printf("    RP: %s\n", reportName);

                        report = LinkedList_getNext(report);
                    }

                    LinkedList_destroy(reports);

                    reports = IedConnection_getLogicalNodeDirectory(con, &error, lnRef,
                            ACSI_CLASS_BRCB);

                    report = LinkedList_getNext(reports);

                    while (report != NULL) {
                        char* reportName = (char*) report->data;

                        printf("    BR: %s\n", reportName);

                        report = LinkedList_getNext(report);
                    }

                    LinkedList_destroy(reports);

                    logicalNode = LinkedList_getNext(logicalNode);
                }

                LinkedList_destroy(logicalNodes);

                device = LinkedList_getNext(device);
            }

            LinkedList_destroy(deviceList);

            IedConnection_close(con);
        }
    }
    else {
        printf("Connection failed!\n");
    }
    printf("End Test\n");
    printf("=========================================\n");
}


void datasetManipulationTests(IedConnection con, IedClientError error, const char* hostname, int tcpPort){
    printf("Begin Test = Dataset Manipulation in the Server...\n");
    IedConnection_connect(con, &error, hostname, tcpPort);

    if (error == IED_ERROR_OK) {

        /* Create a new data set at the remote server */
        LinkedList newDataSetEntries = LinkedList_create();

        LinkedList_add(newDataSetEntries, "simpleIOGenericIO/GGIO1.AnIn1[MX]");
        LinkedList_add(newDataSetEntries, "simpleIOGenericIO/GGIO1.AnIn2[MX]");
        LinkedList_add(newDataSetEntries, "simpleIOGenericIO/GGIO1.AnIn3[MX]");
        LinkedList_add(newDataSetEntries, "simpleIOGenericIO/GGIO1.AnIn4[MX]");

        IedConnection_createDataSet(con, &error, "simpleIOGenericIO/LLN0.AnalogueValues", newDataSetEntries);

        LinkedList_destroyStatic(newDataSetEntries);


        if (error == IED_ERROR_OK ) {

            /* read new data set */
            ClientDataSet clientDataSet;

            clientDataSet = IedConnection_readDataSetValues(con, &error, "simpleIOGenericIO/LLN0.AnalogueValues", NULL);

            if (error == IED_ERROR_OK) {
                printDataSetValues(ClientDataSet_getValues(clientDataSet));

                ClientDataSet_destroy(clientDataSet);
            }
            else {
                printf("Failed to read data set (error code: %d)\n", error);
            }

            IedConnection_deleteDataSet(con, &error, "simpleIOGenericIO/LLN0.AnalogueValues");
        }
        else {
            printf("Failed to create data set (error code: %d)\n", error);
        }

        IedConnection_close(con);
    }
    else {
        printf("Failed to connect to %s:%i\n", hostname, tcpPort);
    }
    printf("End Test\n");
    printf("=========================================\n");
}


int main(int argc, char** argv) {

    char* hostname;
    int tcpPort = 102;

    if (argc > 1)
        hostname = argv[1];
    else
        hostname = "localhost";

    if (argc > 2)
        tcpPort = atoi(argv[2]);

    IedClientError error;
    IedConnection con = IedConnection_create();
    IedConnection_connect(con, &error, hostname, tcpPort);

    if (error == IED_ERROR_OK) {

        /* Test - Read an analog measurement value from server */
        readAnalogMeasurementTests(con, error, hostname, tcpPort);
        Thread_sleep(1000);

        /* Test - Write a variable to the server */
        writeDataToServerTests(con, error, hostname, tcpPort);
        Thread_sleep(1000);

        /* Test - Read data set */
        readDataSetTests(con, error, hostname, tcpPort);
        Thread_sleep(1000);

        /* Test - Reporting */
        reportingTests(con, error, hostname, tcpPort);
        Thread_sleep(1000);

        /* Test - Browse Data Model (file-tool)*/
        browseDataModel(con, error, hostname, tcpPort);
        Thread_sleep(1000);

        /* Test - Browse Data Model (file-tool)*/
        // browseDataModel(con, error, hostname, tcpPort);
        // Thread_sleep(1000);
    }
    else {
        printf("Failed to connect to %s:%i\n", hostname, tcpPort);
    }

    IedConnection_destroy(con);
    return 0;
}
