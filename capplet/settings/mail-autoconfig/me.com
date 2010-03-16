<?xml version="1.0" encoding="UTF-8"?>

<clientConfig>
    <emailProvider id="me.com">
      <domain>mac.com</domain>
      <domain>me.com</domain>

      <displayName>Apple MobileMe</displayName>
      <displayShortName>Apple</displayShortName>

      <incomingServer type="imap">
         <hostname>mail.me.com</hostname>
         <port>993</port>
         <socketType>SSL</socketType>
         <username>%EMAILLOCALPART%</username>
         <authentication>plain</authentication>
      </incomingServer>

      <outgoingServer type="smtp">
         <hostname>smtp.me.com</hostname>
         <port>465</port>
         <socketType>SSL</socketType>
         <username>%EMAILLOCALPART%</username>
         <authentication>plain</authentication>
         <addThisServer>true</addThisServer>
         <useGlobalPreferredServer>false</useGlobalPreferredServer>
      </outgoingServer>

    </emailProvider>
</clientConfig>
