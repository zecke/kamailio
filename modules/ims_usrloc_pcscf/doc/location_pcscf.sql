CREATE TABLE `location` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `domain` varchar(64) DEFAULT NULL,
  `aor` varchar(255) NOT NULL,
  `contact` varchar(255) DEFAULT NULL,
  `received` varchar(128) DEFAULT NULL,
  `received_port` int(10) unsigned DEFAULT NULL,
  `received_proto` int(10) unsigned DEFAULT NULL,
  `path` varchar(512) DEFAULT NULL,
  `rx_session_id` varchar(256) DEFAULT NULL,
  `reg_state` tinyint(4) DEFAULT NULL,
  `expires` datetime DEFAULT '2030-05-28 21:32:15',
  `service_routes` varchar(2048) DEFAULT NULL,
  `socket` varchar(64) DEFAULT NULL,
  `public_ids` varchar(2048) DEFAULT NULL,
  PRIMARY KEY (`id`),
  KEY `expires_idx` (`expires`)
) ENGINE=InnoDB AUTO_INCREMENT=7 DEFAULT CHARSET=latin1;


