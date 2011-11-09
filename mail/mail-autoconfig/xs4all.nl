<?xml version="1.0" encoding="UTF-8"?>

<clientConfig>
    <emailProvider id="xs4all.nl">
      <domain>xs4all.nl</domain>

      <displayName>XS4All</displayName>
      <displayShortName>XS4All</displayShortName>

      <incomingServer type="pop3">
         <hostname>pops.xs4all.nl</hostname>
         <port>995</port>
         <socketType>SSL</socketType>
         <username>%EMAILLOCALPART%</username>
         <authentication>plain</authentication>
      </incomingServer>

      <outgoingServer type="smtp">
         <hostname>smtps.xs4all.nl</hostname>
         <port>465</port>
         <socketType>SSL</socketType>
         <username>%EMAILLOCALPART%</username>
         <authentication>plain</authentication>
         <addThisServer>true</addThisServer>
         <useGlobalPreferredServer>false</useGlobalPreferredServer>
      </outgoingServer>

    </emailProvider>
</clientConfig>
