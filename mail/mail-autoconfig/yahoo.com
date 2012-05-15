<?xml version="1.0" encoding="UTF-8"?>

<clientConfig>
    <emailProvider id="yahoo.com">
      <domain>yahoo.com</domain>
      <domain>yahoo.de</domain>
      <domain>yahoo.it</domain>
      <domain>yahoo.fr</domain>
      <domain>yahoo.co.uk</domain>
      <domain>yahoo.com.br</domain>
      <domain>ymail.com</domain>
      <domain>rocketmail.com</domain>

      <displayName>Yahoo! Mail</displayName>
      <displayShortName>Yahoo</displayShortName>

      <incomingServer type="imap">
         <hostname>imap.mail.yahoo.com</hostname>
         <port>993</port>
         <socketType>SSL</socketType>
         <username>%EMAILADDRESS%</username>
         <authentication>plain</authentication>
      </incomingServer>

      <outgoingServer type="smtp">
         <hostname>smtp.mail.yahoo.com</hostname>
         <port>465</port>
         <socketType>SSL</socketType>
         <username>%EMAILADDRESS%</username>
         <authentication>plain</authentication>
         <addThisServer>true</addThisServer>
         <useGlobalPreferredServer>false</useGlobalPreferredServer>
      </outgoingServer>

    </emailProvider>
</clientConfig>
