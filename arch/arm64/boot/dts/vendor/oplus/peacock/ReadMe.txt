DTSI修改规则：
1.QCOM 路径下的所有修改保持原生态;
2.禁止修改*.dts文件,所有修改放入到*.dtsi文件中;
3.平台共性问题修改放到bengal-beta-overlay.dtsi，或者新建dtsi文件并include到bengal-beta-overlay.dtsi中;
4.项目之间的差异，平台无法共用，需要放入到具体项目的*-overlay.dtsi，
  例如NFC只有20241使用，那么NFC的修改需要放入到bengal-beta-20241-overlay.dtsi
5.具体操作，举例说明：
	a.)要删除qupv3_se1_i2c节点下的nq节点，同时添加自己定义的aw87359_spk_i2c节点
		&qupv3_se1_i2c {
			/delete-node/ nq;/*删除节点*/

			aw87359_spk_i2c@58{/*添加节点*/
				compatible = "awinic,aw87359_spk_pa";
				reg = <0x58>;
				reset-gpio = <&tlmm 111 0x00>;
				pinctrl-names = "spk_reset_on", "spk_reset_off";
				pinctrl-0 = <&aw_spk_reset_on>;
				pinctrl-1 = <&aw_spk_reset_off>;
			};
		};
	b.)需要修改bengal_snd 节点中的qcom,wsa-max-devs属性值到0
		&bengal_snd {
			/delete-property/ qcom,wsa-max-devs;/*删除属性*/
			qcom,wsa-max-devs = <0>;/*添加属性*/
		};
注意：禁止/ { };创建根节点，然后在根节点中修改。