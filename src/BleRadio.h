#include <Arduino.h>
#include <NimBLEDevice.h>
#define BLE_CONFIGURATION_SERVICE_ID "5AB457FD-FBAD-475B-97A0-29900940A47B"
#define BLE_CONFIGURATION_SERVICE_CHAR_ID "7F2D2A4E-BA58-4E8F-8B96-6C8BDCBA629E"
#define BLE_SESSION_SERVICE_ID "176A2A43-0F84-4036-898A-768348A9EC3B"
#define BLE_SESSION_SERVICE_CHAR_ID "78931A77-8177-4679-844A-89BFE2BD0FA9"

class BleRadio : 
    NimBLEClientCallbacks, 
    NimBLEAdvertisedDeviceCallbacks, 
    NimBLEServerCallbacks, 
    NimBLECharacteristicCallbacks, 
    NimBLEDescriptorCallbacks
{
    bool m_initialized;
    NimBLEAdvertisedDevice *m_advDevice;
    bool m_doConnect;
    uint32_t m_scanTime;
    NimBLEServer *m_server;
    uint32_t m_notifyTS;
    void onResult(NimBLEAdvertisedDevice *advertisedDevice)
    {
        Serial.print(F("BLE Advertised Device found: "));
        Serial.println(advertisedDevice->toString().c_str());
        if (advertisedDevice->isAdvertisingService(NimBLEUUID(BLE_CONFIGURATION_SERVICE_ID)))
        {
            Serial.println(F("BLE Found Configuration Service"));
            /** stop scan before connecting */
            NimBLEDevice::getScan()->stop();
            /** Save the device reference in a global for the client to use*/
            m_advDevice = advertisedDevice;
            /** Ready to connect now */
            m_doConnect = true;
        }
    }
    void onConnect(NimBLEClient *pClient)
    {
        Serial.println(F("BLE Connected"));
        /** After connection we should change the parameters if we don't need fast response times.
         *  These settings are 150ms interval, 0 latency, 450ms timout.
         *  Timeout should be a multiple of the interval, minimum is 100ms.
         *  I find a multiple of 3-5 * the interval works best for quick response/reconnect.
         *  Min interval: 120 * 1.25ms = 150, Max interval: 120 * 1.25ms = 150, 0 latency, 60 * 10ms = 600ms timeout
         */
        pClient->updateConnParams(120, 120, 0, 60);
    }

    void onDisconnect(NimBLEClient *pClient)
    {
        Serial.print(pClient->getPeerAddress().toString().c_str());
        Serial.println(F("BLE  Disconnected - Starting scan"));
        NimBLEDevice::getScan()->start(m_scanTime, onScanEnded);
    }

    /** Called when the peripheral requests a change to the connection parameters.
     *  Return true to accept and apply them or false to reject and keep
     *  the currently used parameters. Default will return true.
     */
    bool onConnParamsUpdateRequest(NimBLEClient *pClient, const ble_gap_upd_params *params)
    {
        if (params->itvl_min < 24)
        { /** 1.25ms units */
            return false;
        }
        else if (params->itvl_max > 40)
        { /** 1.25ms units */
            return false;
        }
        else if (params->latency > 2)
        { /** Number of intervals allowed to skip */
            return false;
        }
        else if (params->supervision_timeout > 100)
        { /** 10ms units */
            return false;
        }

        return true;
    }


    void onConnect(NimBLEServer *pServer)
    {
        Serial.println(F("BLE Client connected"));
        Serial.println(F("BLE Multi-connect support: start advertising"));
        NimBLEDevice::startAdvertising();
    };
    /** Alternative onConnect() method to extract details of the connection. 
     *  See: src/ble_gap.h for the details of the ble_gap_conn_desc struct.
     */
    void onConnect(NimBLEServer *pServer, ble_gap_conn_desc *desc)
    {
        Serial.print(F("BLE Client address: "));
        Serial.println(NimBLEAddress(desc->peer_ota_addr).toString().c_str());
        /** We can use the connection handle here to ask for different connection parameters.
         *  Args: connection handle, min connection interval, max connection interval
         *  latency, supervision timeout.
         *  Units; Min/Max Intervals: 1.25 millisecond increments.
         *  Latency: number of intervals allowed to skip.
         *  Timeout: 10 millisecond increments, try for 5x interval time for best results.  
         */
        pServer->updateConnParams(desc->conn_handle, 24, 48, 0, 60);
    };
    void onDisconnect(NimBLEServer *pServer)
    {
        Serial.println(F("BLE Client disconnected - start advertising"));
        NimBLEDevice::startAdvertising();
    };


    void onAuthenticationComplete(ble_gap_conn_desc *desc)
    {
        if( desc->role==BLE_GAP_ROLE_SLAVE) {
            /** Check that encryption was successful, if not we disconnect the client */
            if (!desc->sec_state.encrypted)
            {
                NimBLEDevice::getServer()->disconnect(desc->conn_handle);
                Serial.println(F("BLE Encrypt connection failed - disconnecting client"));
                return;
            }
            Serial.println(F("BLE Starting BLE work!"));
        } else {
            if (!desc->sec_state.encrypted)
            {
                Serial.println(F("BLE Encrypt connection failed - disconnecting"));
                /** Find the client with the connection handle provided in desc */
                NimBLEDevice::getClientByID(desc->conn_handle)->disconnect();
                return;
            }
        }
    };
    void onRead(NimBLECharacteristic *pCharacteristic)
    {
        Serial.print(pCharacteristic->getUUID().toString().c_str());
        Serial.print(F("BLE : onRead(), value: "));
        Serial.println(pCharacteristic->getValue().c_str());
    };

    void onWrite(NimBLECharacteristic *pCharacteristic)
    {
        Serial.print(F("BLE "));
        Serial.print(pCharacteristic->getUUID().toString().c_str());
        Serial.print(F(": onWrite(), value: "));
        Serial.println(pCharacteristic->getValue().c_str());
    };
    /** Called before notification or indication is sent, 
     *  the value can be changed here before sending if desired.
     */
    void onNotify(NimBLECharacteristic *pCharacteristic)
    {
        Serial.println(F("BLE Sending notification to clients"));
    };

    /** The status returned in status is defined in NimBLECharacteristic.h.
     *  The value returned in code is the NimBLE host return code.
     */
    void onStatus(NimBLECharacteristic *pCharacteristic, Status status, int code)
    {
        Serial.print(F("BLE Notification/Indication status code: "));
        Serial.print(status);
        Serial.print(F(", return code: "));
        Serial.print(code);
        Serial.print(F(", "));
        Serial.println(NimBLEUtils::returnCodeToString(code));
    };

    void onSubscribe(NimBLECharacteristic *pCharacteristic, ble_gap_conn_desc *desc, uint16_t subValue)
    {
        Serial.print(F("Client ID: "));
        Serial.print(desc->conn_handle);
        Serial.print(F(" Address: "));
        Serial.print(std::string(NimBLEAddress(desc->peer_ota_addr)).c_str());
        if (subValue == 0)
        {
            Serial.print(F(" Unsubscribed to "));
        }
        else if (subValue == 1)
        {
            Serial.print(F(" Subscribed to notfications for "));
        }
        else if (subValue == 2)
        {
            Serial.print(F(" Subscribed to indications for "));
        }
        else if (subValue == 3)
        {
            Serial.print(F(" Subscribed to notifications and indications for "));
        }
        Serial.println(std::string(pCharacteristic->getUUID()).c_str());
    };
    void onWrite(NimBLEDescriptor *pDescriptor)
    {
        std::string dscVal((char *)pDescriptor->getValue(), pDescriptor->getLength());
        Serial.print(F("BLE Descriptor witten value:"));
        Serial.println(dscVal.c_str());
    };

    void onRead(NimBLEDescriptor *pDescriptor)
    {
        Serial.print(pDescriptor->getUUID().toString().c_str());
        Serial.println(F("BLE  Descriptor read"));
    };

    /** Notification / Indication receiving handler callback */
    static void onSNotify(NimBLERemoteCharacteristic *pRemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
    {
        NimBLERemoteService* pSvc = pRemoteCharacteristic->getRemoteService();
        if(pRemoteCharacteristic->getUUID()==NimBLEUUID(BLE_CONFIGURATION_SERVICE_CHAR_ID)) {
            std::string val = pRemoteCharacteristic->getValue();
            if(1==val.length() && val.data()[0]==0) {
                Serial.println(F("BLE Keep-alive ping from configuration service"));
                return;
            }
        }
        if(isNotify) {
            Serial.print(F("BLE Notification from "));
        } else {
            Serial.print(F("BLE Indication from "));
        }
        Serial.print(std::string(pSvc->getClient()->getPeerAddress()).c_str());
        Serial.print(F(": Service = "));
        Serial.print(std::string(pSvc->getUUID()).c_str());
        Serial.print(F(", Characteristic = "));
        Serial.print(std::string(pRemoteCharacteristic->getUUID()).c_str());
        Serial.print(F(", Value = "));
        Serial.println(std::string((char *)pData, length).c_str());
    }

    /** Callback to process the results of the last scan or restart it */
    static void onScanEnded(NimBLEScanResults results)
    {
        Serial.println(F("BLE Scan Ended"));
    }
    /** Handles the provisioning of clients and connects / interfaces with the server */
    bool connectToServer()
    {
        NimBLEClient *pClient = nullptr;

        /** Check if we have a client we should reuse first **/
        if (NimBLEDevice::getClientListSize())
        {
            /** Special case when we already know this device, we send false as the
         *  second argument in connect() to prevent refreshing the service database.
         *  This saves considerable time and power.
         */
            pClient = NimBLEDevice::getClientByPeerAddress(m_advDevice->getAddress());
            if (pClient)
            {
                if (!pClient->connect(m_advDevice, false))
                {
                    Serial.println(F("BLE Reconnect failed"));
                    return false;
                }
                Serial.println(F("BLE Reconnected client"));
            }
            /** We don't already have a client that knows this device,
         *  we will check for a client that is disconnected that we can use.
         */
            else
            {
                pClient = NimBLEDevice::getDisconnectedClient();
            }
        }

        /** No client to reuse? Create a new one. */
        if (!pClient)
        {
            if (NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS)
            {
                Serial.println(F("BLE Max clients reached - no more connections available"));
                return false;
            }

            pClient = NimBLEDevice::createClient();

            Serial.println(F("BLE New client created"));

            pClient->setClientCallbacks((NimBLEClientCallbacks *)this, false);
            /** Set initial connection parameters: These settings are 15ms interval, 0 latency, 120ms timout.
         *  These settings are safe for 3 clients to connect reliably, can go faster if you have less
         *  connections. Timeout should be a multiple of the interval, minimum is 100ms.
         *  Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 51 * 10ms = 510ms timeout
         */
            pClient->setConnectionParams(12, 12, 0, 51);
            /** Set how long we are willing to wait for the connection to complete (seconds), default is 30. */
            pClient->setConnectTimeout(5);

            if (!pClient->connect(m_advDevice))
            {
                /** Created a client but failed to connect, don't need to keep it as it has no data */
                NimBLEDevice::deleteClient(pClient);
                Serial.println(F("BLE Failed to connect, deleted client"));
                return false;
            }
        }

        if (!pClient->isConnected())
        {
            if (!pClient->connect(m_advDevice))
            {
                Serial.println(F("BLE Failed to connect"));
                return false;
            }
        }

        Serial.print(F("BLE Connected to: "));
        Serial.println(pClient->getPeerAddress().toString().c_str());
        Serial.print(F("BLE RSSI: "));
        Serial.println(pClient->getRssi());

        /** Now we can read/write/subscribe the charateristics of the services we are interested in */
        NimBLERemoteService *pSvc = nullptr;
        NimBLERemoteCharacteristic *pChr = nullptr;
        NimBLERemoteDescriptor *pDsc = nullptr;

        pSvc = pClient->getService(BLE_CONFIGURATION_SERVICE_ID);
        if (pSvc)
        { /** make sure it's not null */
            pChr = pSvc->getCharacteristic(BLE_CONFIGURATION_SERVICE_CHAR_ID);

            if (pChr)
            { /** make sure it's not null */
                if (pChr->canRead())
                {
                    Serial.print(F("BLE "));
                    Serial.print(pChr->getUUID().toString().c_str());
                    Serial.print(F(" Value: "));
                    Serial.println(pChr->readValue().c_str());
                }

                pDsc = pChr->getDescriptor(NimBLEUUID("C01D"));
                if (pDsc)
                { /** make sure it's not null */
                    Serial.print(F("BLE Descriptor: "));
                    Serial.print(pDsc->getUUID().toString().c_str());
                    Serial.print(F("BLE  Value: "));
                    Serial.println(pDsc->readValue().c_str());
                }

                if (pChr->canWrite())
                {
                    if (pChr->writeValue("No tip!"))
                    {
                        Serial.print(F("BLE Wrote new value to: "));
                        Serial.println(pChr->getUUID().toString().c_str());
                    }
                    else
                    {
                        /** Disconnect if write failed */
                        pClient->disconnect();
                        return false;
                    }

                    if (pChr->canRead())
                    {
                        Serial.print(F("BLE The value of: "));
                        Serial.print(pChr->getUUID().toString().c_str());
                        Serial.print(F(" is now: "));
                        Serial.println(pChr->readValue().c_str());
                    }
                }

                /** registerForNotify() has been deprecated and replaced with subscribe() / unsubscribe().
             *  Subscribe parameter defaults are: notifications=true, notifyCallback=nullptr, response=false.
             *  Unsubscribe parameter defaults are: response=false.
             */
                if (pChr->canNotify())
                {
                    //if(!pChr->registerForNotify(notifyCB)) {
                    if (!pChr->subscribe(true, onSNotify))
                    {
                        /** Disconnect if subscribe failed */
                        pClient->disconnect();
                        return false;
                    }
                }
                else if (pChr->canIndicate())
                {
                    /** Send false as first argument to subscribe to indications instead of notifications */
                    //if(!pChr->registerForNotify(notifyCB, false)) {
                    if (!pChr->subscribe(false, onSNotify))
                    {
                        /** Disconnect if subscribe failed */
                        pClient->disconnect();
                        return false;
                    }
                }
            }
        }
        else
        {
            Serial.println(F("BLE Configuration service not found."));
        }

        return true;
    }

public:
    bool begin()
    {
        if (m_initialized)
        {
            NimBLEDevice::deinit(true);
        }
        m_initialized = false;
        m_scanTime = 0;
        m_advDevice = nullptr;
        m_doConnect = false;
        m_server = nullptr;
        m_notifyTS=0;
        return true;
    }
    bool off()
    {
        if (!m_initialized)
        {
            Serial.println(F("BLE Radio not on"));
            return false;
        }
        NimBLEDevice::deinit(true);
        m_initialized = false;
        Serial.println(F("BLE Radio off"));
        return true;
    }
    bool on(const char *deviceName, esp_power_level_t powerLevel = ESP_PWR_LVL_P9, bool activeScan = true, uint8_t authRec = BLE_SM_PAIR_AUTHREQ_SC)
    {
        if (nullptr == deviceName)
            deviceName = "";
        if (m_initialized)
        {
            Serial.println(F("BLE Radio already on"));
            return false;
        }
        NimBLEDevice::init(deviceName);

        NimBLEDevice::setPower(powerLevel);

        /** Set the IO capabilities of the device, each option will trigger a different pairing method.
             *  BLE_HS_IO_DISPLAY_ONLY    - Passkey pairing
             *  BLE_HS_IO_DISPLAY_YESNO   - Numeric comparison pairing
             *  BLE_HS_IO_NO_INPUT_OUTPUT - DEFAULT setting - just works pairing
             */
        //NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY); // use passkey
        //NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO); //use numeric comparison

        /** 2 different ways to set security - both calls achieve the same result.
         *  no bonding, no man in the middle protection, secure connections.
         *   
         *  These are the default values, only shown here for demonstration.   
         */
        //NimBLEDevice::setSecurityAuth(false, false, true);
        NimBLEDevice::setSecurityAuth(authRec);

        Serial.println(F("BLE Creating session server"));
        m_server = NimBLEDevice::createServer();
        if (nullptr == m_server)
        {
            Serial.println(F("BLE Error session creating server"));
            return false;
        }
        m_server->setCallbacks((NimBLEServerCallbacks *)this, false);

        NimBLEService *pDeadService = m_server->createService(BLE_SESSION_SERVICE_ID);
        if (nullptr == pDeadService)
        {
            Serial.println(F("BLE Error creating session service"));
            return false;
        }
        NimBLECharacteristic *pBeefCharacteristic = pDeadService->createCharacteristic(
            BLE_SESSION_SERVICE_CHAR_ID,
            NIMBLE_PROPERTY::READ |
                NIMBLE_PROPERTY::WRITE |
                /** Require a secure connection for read and write access */
                NIMBLE_PROPERTY::READ_ENC | // only allow reading if paired / encrypted
                NIMBLE_PROPERTY::WRITE_ENC  // only allow writing if paired / encrypted
        );

        pBeefCharacteristic->setValue("Burger");
        pBeefCharacteristic->setCallbacks((NimBLECharacteristicCallbacks *)this);

        /** 2904 descriptors are a special case, when createDescriptor is called with
         *  0x2904 a NimBLE2904 class is created with the correct properties and sizes.
         *  However we must cast the returned reference to the correct type as the method
         *  only returns a pointer to the base NimBLEDescriptor class.
         */
        NimBLE2904 *pBeef2904 = (NimBLE2904 *)pBeefCharacteristic->createDescriptor("2904");
        pBeef2904->setFormat(NimBLE2904::FORMAT_UTF8);
        pBeef2904->setCallbacks((NimBLEDescriptorCallbacks *)this);

        /** Start the services when finished creating all Characteristics and Descriptors */
        if (!pDeadService->start())
        {
            Serial.println(F("BLE Error starting session service"));
            return false;
        }
        

        NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
        if (nullptr == pAdvertising)
        {
            Serial.println(F("BLE Error creating session advertising object"));
            return false;
        }
        /** Add the services to the advertisment data **/
        pAdvertising->addServiceUUID(pDeadService->getUUID());
        
        /** If your device is battery powered you may consider setting scan response
         *  to false as it will extend battery life at the expense of less data sent.
         */
        pAdvertising->setScanResponse(true);
        if (!pAdvertising->start())
        {
            Serial.println(F("BLE Error starting advertising"));
            return false;
        }

        Serial.println(F("BLE Advertising Started"));

        /** create new scan */
        NimBLEScan *pScan = NimBLEDevice::getScan();
        if (nullptr == pScan)
        {
            Serial.println(F("BLE Error creating scan object"));
            return false;
        }
        /** create a callback that gets called when advertisers are found */
        pScan->setAdvertisedDeviceCallbacks((NimBLEAdvertisedDeviceCallbacks *)this, false);

        /** Set scan interval (how often) and window (how long) in milliseconds */
        pScan->setInterval(45);
        pScan->setWindow(15);

        /** Active scan will gather scan response data from advertisers
         *  but will use more energy from both devices
         */
        pScan->setActiveScan(activeScan);
        /** Start scanning for advertisers for the scan time specified (in seconds) 0 = forever
         *  Optional callback for when scanning stops.
         */
        if (pScan->start(m_scanTime, onScanEnded))
        {
            Serial.println(F("BLE Scan started"));
        }
        else
        {
            Serial.println(F("BLE Scan error"));
            return false;
        }
        return true;
    }
    void update()
    {
        if(m_doConnect) {
            m_doConnect = false;
            
            /** Found a device we want to connect to, do it now */
            if (connectToServer())
            {
                Serial.println(F("BLE Success! we should now be getting notifications, scanning for more!"));
            }
            else
            {
                Serial.println(F("BLE Failed to connect, starting scan"));
            }

            NimBLEDevice::getScan()->start(m_scanTime, onScanEnded);
        
        
            
        }
        

        if (100 < millis() - m_notifyTS)
        {
            m_notifyTS = millis();
            return;
            /** Do your thing here, this just spams notifications to all connected clients */
            if (m_server->getConnectedCount())
            {
                NimBLEService *pSvc = m_server->getServiceByUUID(BLE_SESSION_SERVICE_ID);
                if (pSvc)
                {
                    NimBLECharacteristic *pChr = pSvc->getCharacteristic(BLE_SESSION_SERVICE_CHAR_ID);
                    if (pChr)
                    {
                        pChr->notify(true);
                    }
                }
            }
        }
    }
};
static BleRadio g_ble;