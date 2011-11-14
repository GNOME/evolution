<?xml version="1.0" encoding="UTF-8"?>

<clientConfig>
    <emailProvider id="googlemail.com">
      <domain>gmail.com</domain>
      <domain>googlemail.com</domain>

      <displayName>Google Mail</displayName>
      <displayShortName>GMail</displayShortName>

      <incomingServer type="imap">
         <hostname>imap.googlemail.com</hostname>
         <port>993</port>
         <socketType>SSL</socketType>
         <username>%EMAILADDRESS%</username>
         <authentication>plain</authentication>
      </incomingServer>

      <outgoingServer type="smtp">
         <hostname>smtp.googlemail.com</hostname>
         <port>465</port>
         <socketType>SSL</socketType>
         <username>%EMAILADDRESS%</username>
         <authentication>plain</authentication>
         <addThisServer>true</addThisServer>
         <useGlobalPreferredServer>false</useGlobalPreferredServer>
      </outgoingServer>

      <enableURL url="https://mail.google.com/mail/?ui=2&amp;shva=1#settings/fwdandpop">You need to enable IMAP access</enableURL>

    </emailProvider>
</clientConfig>
