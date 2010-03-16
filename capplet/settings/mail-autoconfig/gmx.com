<?xml version="1.0" encoding="UTF-8"?>
<clientConfig>
  <emailProvider id="gmx.com">
    <domain>gmx.com</domain>
    <displayName>GMX Freemail</displayName>
    <displayShortName>GMX</displayShortName>
    <incomingServer type="imap">
      <hostname>imap.gmx.com</hostname>
      <port>993</port>
      <socketType>SSL</socketType>
      <username>%EMAILADDRESS%</username>
      <authentication>plain</authentication>
    </incomingServer>
    <outgoingServer type="smtp">
      <hostname>mail.gmx.com</hostname>
      <port>465</port>
      <socketType>SSL</socketType>
      <username>%EMAILADDRESS%</username>
      <authentication>plain</authentication>
      <addThisServer>true</addThisServer>
      <useGlobalPreferredServer>false</useGlobalPreferredServer>
    </outgoingServer>
  </emailProvider>
</clientConfig>
