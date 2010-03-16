<?xml version="1.0" encoding="UTF-8"?>

<clientConfig>
    <emailProvider id="aol.com">
      <domain>aol.com</domain>
      <domain>aim.com</domain>

      <displayName>AOL</displayName>
      <displayShortName>AOL</displayShortName>

      <incomingServer type="imap">
         <hostname>imap.aol.com</hostname>
         <port>143</port>
         <socketType>plain</socketType>
         <username>%EMAILLOCALPART%</username>
         <authentication>plain</authentication>
      </incomingServer>

      <outgoingServer type="smtp">
         <hostname>smtp.aol.com</hostname>
         <port>587</port>
         <socketType>plain</socketType>
         <username>%EMAILLOCALPART%</username>
         <authentication>plain</authentication>
         <addThisServer>true</addThisServer>
         <useGlobalPreferredServer>false</useGlobalPreferredServer>
      </outgoingServer>

    </emailProvider>
</clientConfig>
