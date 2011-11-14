<?xml version="1.0" encoding="UTF-8"?>

<clientConfig>
    <emailProvider id="peoplepc.com">
      <domain>peoplepc.com</domain>

      <displayName>PeoplePC</displayName>
      <displayShortName>PeoplePC</displayShortName>

      <incomingServer type="imap">
         <hostname>imap.peoplepc.com</hostname>
         <port>143</port>
         <socketType>plain</socketType>
         <username>%EMAILADDRESS%</username>
         <authentication>secure</authentication>
      </incomingServer>

      <outgoingServer type="smtp">
         <hostname>smtpauth.peoplepc.com</hostname>
         <port>587</port>
         <socketType>plain</socketType>
         <username>%EMAILADDRESS%</username>
         <authentication>plain</authentication>
         <addThisServer>true</addThisServer>
         <useGlobalPreferredServer>false</useGlobalPreferredServer>
      </outgoingServer>

    </emailProvider>
</clientConfig>
